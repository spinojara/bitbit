/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/random.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sqlite3.h>

#include "testbitshared.h"
#include "util.h"
#include "sprt.h"

#define BACKLOG 10

#define NRM "\x1B[0m"
#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YLW "\x1B[33m"
#define MGT "\x1B[35m"
#define CYN "\x1B[36m"

enum {
	PASSWORD,
	AWAITING,
	RUNNING,
};

struct connection {
	char buf[128];
	int status;
	int id;
	double elo0;
	double elo1;

	size_t len;
	char *patch;
};

int get_listener_socket(const char *port) {
	int listener;
	int yes = 1;
	int rv;

	struct addrinfo hints = { 0 }, *ai, *p;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, port, &hints, &ai))) {
		fprintf(stderr, "error: %s\n", gai_strerror(rv));
		exit(1);
	}

	for (p = ai; p; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
			continue;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	freeaddrinfo(ai);

	if (!p)
		return -1;

	if (listen(listener, BACKLOG) == -1)
		return -1;

	return listener;
}

struct connection *add_to_pdfs(struct pollfd *pdfs[], struct connection *connections[], int newfd, int *fd_count, int *fd_size) {
	if (*fd_count == *fd_size) {
		*fd_size *= 2;
		*pdfs = realloc(*pdfs, *fd_size * sizeof(**pdfs));
		*connections = realloc(*connections, *fd_size * sizeof(**connections));
	}
	memset(&(*pdfs)[*fd_count], 0, sizeof((*pdfs)[*fd_count]));
	memset(&(*connections)[*fd_count], 0, sizeof(**connections));

	(*pdfs)[*fd_count].fd = newfd;
	(*pdfs)[*fd_count].events = POLLIN;

	(*connections)[*fd_count].status = PASSWORD;

	return &(*connections)[(*fd_count)++];
}

void del_from_pdfs(struct pollfd pdfs[], struct connection connections[], int i, int *fd_count) {
	/* Copy the last element here. */
	pdfs[i] = pdfs[*fd_count - 1];
	connections[i] = connections[*fd_count - 1];
	--*fd_count;
}

int main(int argc, char **argv) {
	char password[128];
	getpassword(password);

	char *port = "2718";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			i++;
			if (!(i < argc))
				break;
			port = argv[i];
		}
	}

	int listener;
	int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	char buf[BUFSIZ] = { 0 }, *str;
	int n;
	
	int fd_count = 0;
	int fd_size = 4;
	struct pollfd *pdfs = malloc(fd_size * sizeof(*pdfs));
	struct connection *connections = malloc(fd_size * sizeof(*connections));

	listener = get_listener_socket(port);
	if (listener == -1) {
		fprintf(stderr, "error: getting listening socket\n");
		return 1;
	}

	pdfs[0].fd = listener;
	pdfs[0].events = POLLIN;
	connections[0].status = -1;
	fd_count = 1;

	sqlite3 *db;
	sqlite3_stmt *stmt, *stmt2;
	sqlite3_blob *blob;
	int r;
	sqlite3_open("/var/lib/testbit/testbit.db", &db);
	if (!db) {
		fprintf(stderr, "error: failed to open /var/lib/testbit/testbit.db");
		return 1;
	}

	r = sqlite3_exec(db,
			"CREATE TABLE IF NOT EXISTS tests ("
			"id        INTEGER PRIMARY KEY, "
			"status    INTEGER, "
			"elo0      REAL, "
			"elo1      REAL, "
			"queuetime INTEGER, "
			"starttime INTEGER, "
			"donetime  INTEGER, "
			"elo       REAL, "
			"pm        REAL, "
			"result    INTEGER, "
			"t0        INTEGER, "
			"t1        INTEGER, "
			"t2        INTEGER, "
			"p0        INTEGER, "
			"p1        INTEGER, "
			"p2        INTEGER, "
			"p3        INTEGER, "
			"p4        INTEGER, "
			"patch     BLOB"
			");",
			NULL, NULL, NULL);
	if (r) {
		fprintf(stderr, "error: failed to create table tests\n");
		return 1;
	}

	/* Requeue all tests that ran before closing. */
	sqlite3_prepare_v2(db,
			"UPDATE tests SET status = ? WHERE status = ?;",
			-1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, TESTQUEUE);
	sqlite3_bind_int(stmt, 2, TESTRUNNING);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	while (1) {
		/* Loop through queue and start available tests. */
		r = sqlite3_prepare_v2(db,
				"SELECT id, elo0, elo1 FROM tests WHERE status = ? ORDER BY queuetime ASC;",
				-1, &stmt, NULL);
		sqlite3_bind_int(stmt, 1, TESTQUEUE);

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			for (int i = 0; i < fd_count; i++) {
				int fd = pdfs[i].fd;
				struct connection *connection = &connections[i];
				if (connection->status == AWAITING) {
					connection->status = RUNNING;
					connection->id = sqlite3_column_int(stmt, 0);
					connection->elo0 = sqlite3_column_double(stmt, 1);
					connection->elo1 = sqlite3_column_double(stmt, 2);
					sqlite3_prepare_v2(db,
							"UPDATE tests SET starttime = unixepoch(), status = ? "
							"WHERE id = ?;",
							-1, &stmt2, NULL);
					sqlite3_bind_int(stmt2, 1, TESTRUNNING);
					sqlite3_bind_int(stmt2, 2, connection->id);
					sqlite3_step(stmt2);
					sqlite3_finalize(stmt2);

					sqlite3_blob_open(db, "main", "tests", "patch", connection->id, 1, &blob);
					connection->len = sqlite3_blob_bytes(blob);
					connection->patch = malloc(connection->len);
					sqlite3_blob_read(blob, connection->patch, connection->len, 0);
					sqlite3_blob_close(blob);

					double elo[2] = { connection->elo0, connection->elo1 };

					sendall(fd, (char *)elo, 16);
					sendall(fd, connection->patch, connection->len);
					sendall(fd, "\0", 1);

					free(connection->patch);
				}
			}
		}
		sqlite3_finalize(stmt);

		int poll_count = poll(pdfs, fd_count, -1);

		if (poll_count == -1) {
			fprintf(stderr, "error: poll\n");
			return 1;
		}

		for (int i = 0; i < fd_count; i++) {
			if (pdfs[i].revents & POLLIN) {
				addrlen = sizeof(remoteaddr);
				if (pdfs[i].fd == listener) {
					newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
					printf("New connection %d\n", newfd);
					if (newfd == -1) {
						fprintf(stderr, "error: accept\n");
					}
					else {
						/* Set timeout so that recvexact timeouts if a message
						 * of the wrong length is sent. This is given in milliseconds.
						 */
						struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
						setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

						if (recvexact(newfd, buf, 1) || (buf[0] != CLIENT && buf[0] != NODE && buf[0] != LOG)) {
							str = "error: bad type\n";
							sendall(newfd, str, strlen(str));
							close(newfd);
							continue;
						}

						if (buf[0] == LOG) {
							/* Send logs. */
							sqlite3_prepare_v2(db,
									"SELECT "
									"id, status, elo0, elo1, queuetime, starttime, donetime, "
									"elo, pm, result, t0, t1, t2, p0, p1, p2, p3, p4 "
									"FROM tests ORDER BY queuetime ASC;",
									-1, &stmt, NULL);

							int first = 1;
							while (sqlite3_step(stmt) == SQLITE_ROW) {
								int id = sqlite3_column_int(stmt, 0);
								int status = sqlite3_column_int(stmt, 1);
								double elo0 = sqlite3_column_double(stmt, 2);
								double elo1 = sqlite3_column_double(stmt, 3);
								time_t queuetime = sqlite3_column_int(stmt, 4);
								time_t starttime = sqlite3_column_int(stmt, 5);
								time_t donetime = sqlite3_column_int(stmt, 6);
								double elo = sqlite3_column_double(stmt, 7);
								double pm = sqlite3_column_double(stmt, 8);
								int H = sqlite3_column_int(stmt, 9);
								int t0 = sqlite3_column_int(stmt, 10);
								int t1 = sqlite3_column_int(stmt, 11);
								int t2 = sqlite3_column_int(stmt, 12);
								int p0 = sqlite3_column_int(stmt, 13);
								int p1 = sqlite3_column_int(stmt, 14);
								int p2 = sqlite3_column_int(stmt, 15);
								int p3 = sqlite3_column_int(stmt, 16);
								int p4 = sqlite3_column_int(stmt, 17);

								if (!first)
									sendall(newfd, "\n", 1);
								first = 0;

								char timebuf[3][26];
								switch (status) {
								case TESTQUEUE:
									sprintf(buf, YLW "Status      Queue\n"
											 "H0          Elo < %lf\n"
											 "H1          Elo > %lf\n"
											 "Queue       %s",
											 elo0, elo1,
											 ctime_r(&queuetime, timebuf[0]));
									break;
								case TESTRUNNING:
									sprintf(buf, MGT "Status      Running\n"
											 "H0          Elo < %lf\n"
											 "H1          Elo > %lf\n"
											 "Queue       %s"
											 "Start       %s",
											 elo0, elo1,
											 ctime_r(&queuetime, timebuf[0]),
											 ctime_r(&starttime, timebuf[1]));
									break;
								case TESTDONE:
									sprintf(buf, GRN "Status      Done\n"
											 "H0          Elo < %lf\n"
											 "H1          Elo > %lf\n"
											 "Queue       %s"
											 "Start       %s"
											 "Done        %s"
											 "Games       %d\n"
											 "Trinomial   %d - %d - %d\n"
											 "Pentanomial %d - %d - %d - %d - %d\n"
											 "Elo         %lf +- %lf\n"
											 "Result      %s\n",
											 elo0, elo1,
											 ctime_r(&queuetime, timebuf[0]),
											 ctime_r(&starttime, timebuf[1]),
											 ctime_r(&donetime, timebuf[2]),
											 t0 + t1 + t2, t0, t1, t2,
											 p0, p1, p2, p3, p4, elo, pm,
											 H == H0 ? RED "H0 accepted" : H == H1 ? "H1 accepted" : YLW "Inconclusive");
									break;
								case RUNERROR:
									sprintf(buf, RED "Status      Runtime Error\n"
											 "H0          Elo < %lf\n"
											 "H1          Elo > %lf\n"
											 "Queue       %s"
											 "Start       %s"
											 "Done        %s"
											 "Games       %d\n"
											 "Trinomial   %d - %d - %d\n"
											 "Pentanomial %d - %d - %d - %d - %d\n",
											 elo0, elo1,
											 ctime_r(&queuetime, timebuf[0]),
											 ctime_r(&starttime, timebuf[1]),
											 ctime_r(&donetime, timebuf[2]),
											 t0 + t1 + t2, t0, t1, t2,
											 p0, p1, p2, p3, p4);
									break;
								case PATCHERROR:
								case MAKEERROR:
									sprintf(buf, RED "Status      %s\n"
											 "H0          Elo < %lf\n"
											 "H1          Elo > %lf\n"
											 "Queue       %s"
											 "Start       %s",
											 status == PATCHERROR ? "Patch Error" : "Make Error",
											 elo0, elo1,
											 ctime_r(&queuetime, timebuf[0]),
											 ctime_r(&starttime, timebuf[1]));
								}
								sendall(newfd, buf, strlen(buf));

								/* And now send the patch. */
								sendall(newfd, CYN, strlen(CYN));
								str = "================================================\n";
								sendall(newfd, str, strlen(str));
								sqlite3_blob_open(db, "main", "tests", "patch", id, 1, &blob);
								int len = sqlite3_blob_bytes(blob);
								char *patch = malloc(len);
								sqlite3_blob_read(blob, patch, len, 0);
								sqlite3_blob_close(blob);

								sendall(newfd, patch, len);

								free(patch);

								sendall(newfd, str, strlen(str));
							}
							sqlite3_finalize(stmt);
							close(newfd);
							continue;
						}

						struct connection *newconnection;
						newconnection = add_to_pdfs(&pdfs, &connections, newfd, &fd_count, &fd_size);
						/* Temporarily save the connection type here. */
						newconnection->id = buf[0];

						char salt[32];
						getrandom(salt, sizeof(salt), 0);
						memcpy(newconnection->buf, password, 128);
						hashpassword(newconnection->buf, salt);
						sendall(newfd, salt, 32);

						printf("status password: %d\n", newconnection->status == PASSWORD);
					}
				}
				else {
					struct connection *connection = &connections[i];
					int fd = pdfs[i].fd;

					uint64_t trinomial[3];
					uint64_t pentanomial[5];
					char H;
					double elo, pm;
					int status;

					memset(buf, 0, sizeof(buf));
					switch (connection->status) {
					case PASSWORD:
						if (recvexact(fd, buf, 64) || strcmp(connection->buf, buf)) {
							if (connection->id == CLIENT) {
								str = "Permission denied\n";
								sendall(fd, str, strlen(str));
							}
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}

						if (connection->id == NODE) {
							connection->status = AWAITING;
							break;
						}

						/* Architecture dependent. */
						if (recvexact(fd, buf, 16)) {
							str = "error: bad constants\n";
							r = sendall(fd, str, strlen(str));
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						connection->elo0 = *(double *)buf;
						connection->elo1 = *(double *)(&buf[8]);
						connection->len = 0;
						connection->patch = NULL;
						while ((n = recv(fd, buf, sizeof(buf) - 1, 0)) > 0) {
							int len = strlen(buf);
							connection->patch = realloc(connection->patch, len + connection->len);
							memcpy(connection->patch + connection->len, buf, len);
							connection->len += len;
							if (len < n)
								break;

							memset(buf, 0, sizeof(buf));
						}
						if (n <= 0) {
							str = "error: bad send\n";
							sendall(fd, str, strlen(str));
							break;
						}

						/* Queue this test. */
						sqlite3_prepare_v2(db,
								"INSERT INTO tests (status, elo0, elo1, queuetime, patch)"
								"VALUES (?, ?, ?, unixepoch(), ?) RETURNING id;",
								-1, &stmt, NULL);
						sqlite3_bind_int(stmt, 1, TESTQUEUE);
						sqlite3_bind_double(stmt, 2, connection->elo0);
						sqlite3_bind_double(stmt, 3, connection->elo1);
						sqlite3_bind_zeroblob(stmt, 4, connection->len);
						sqlite3_step(stmt);
						connection->id = sqlite3_column_int(stmt, 0);
						sqlite3_step(stmt);
						sqlite3_finalize(stmt);

						sqlite3_blob_open(db, "main", "tests", "patch", connection->id, 1, &blob);
						sqlite3_blob_write(blob, connection->patch, connection->len, 0);
						sqlite3_blob_close(blob);

						free(connection->patch);

						sqlite3_prepare_v2(db,
								"SELECT COUNT(*) FROM tests WHERE status = ?;",
								-1, &stmt, NULL);
						sqlite3_bind_int(stmt, 1, TESTQUEUE);
						sqlite3_step(stmt);
						int queue = sqlite3_column_int(stmt, 0);
						sqlite3_step(stmt);
						sqlite3_finalize(stmt);

						int nodes = 0;
						for (int j = 0; j < fd_count; j++)
							if (connections[j].status == AWAITING)
								nodes++;

						sprintf(buf, "Test with id %d has been put in queue. "
							     "There are currently %d tests in queue with "
							     "%d available nodes.\n",
							     connection->id, queue, nodes);
						sendall(fd, buf, strlen(buf));
						del_from_pdfs(pdfs, connections, i, &fd_count);
						close(fd);
						break;
					case AWAITING:
						/* A node should not send anything while awaiting. */
						del_from_pdfs(pdfs, connections, i, &fd_count);
						close(fd);
						break;
					case RUNNING:
						if (recvexact(fd, buf, 1) || (buf[0] != TESTDONE && buf[0] != PATCHERROR && buf[0] != MAKEERROR)) {
							/* Requeue the test if the node closes. */
							sqlite3_prepare_v2(db,
									"UPDATE tests SET status = ? WHERE id = ?;",
									-1, &stmt, NULL);
							sqlite3_bind_int(stmt, 1, TESTQUEUE);
							sqlite3_bind_int(stmt, 2, connection->id);
							sqlite3_step(stmt);
							sqlite3_finalize(stmt);
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						switch (buf[0]) {
						case TESTDONE:
							status = TESTDONE;
							 /* Read hypothesis, trinomial and pentanomial. */
							if (recvexact(fd, (char *)trinomial, 3 * sizeof(*trinomial)) || recvexact(fd, (char *)pentanomial, 5 * sizeof(*pentanomial)) || recvexact(fd, &H, 1)) {
								sqlite3_prepare_v2(db,
										"UPDATE tests SET status = ? WHERE id = ?;",
										-1, &stmt, NULL);
								sqlite3_bind_int(stmt, 1, TESTQUEUE);
								sqlite3_bind_int(stmt, 2, connection->id);
								sqlite3_step(stmt);
								sqlite3_finalize(stmt);
								del_from_pdfs(pdfs, connections, i, &fd_count);
								close(fd);
								break;
							}
							
							if (H != HERROR) {
								elo = sprt_elo(pentanomial, &pm);
							}
							else {
								elo = pm = 0.0;
								status = RUNERROR;
							}

							sqlite3_prepare_v2(db,
									"UPDATE tests SET "
									"status = ?, "
									"donetime = unixepoch(), "
									"elo = ?, "
									"pm = ?, "
									"result = ?, "
									"t0 = ?, "
									"t1 = ?, "
									"t2 = ?, "
									"p0 = ?, "
									"p1 = ?, "
									"p2 = ?, "
									"p3 = ?, "
									"p4 = ? "
									"WHERE id = ?;",
									-1, &stmt, NULL);
							sqlite3_bind_int(stmt, 1, status);
							sqlite3_bind_double(stmt, 2, elo);
							sqlite3_bind_double(stmt, 3, pm);
							sqlite3_bind_int(stmt, 4, H);
							sqlite3_bind_int(stmt, 5, trinomial[0]);
							sqlite3_bind_int(stmt, 6, trinomial[1]);
							sqlite3_bind_int(stmt, 7, trinomial[2]);
							sqlite3_bind_int(stmt, 8, pentanomial[0]);
							sqlite3_bind_int(stmt, 9, pentanomial[1]);
							sqlite3_bind_int(stmt, 10, pentanomial[2]);
							sqlite3_bind_int(stmt, 11, pentanomial[3]);
							sqlite3_bind_int(stmt, 12, pentanomial[4]);
							sqlite3_bind_int(stmt, 13, connection->id);
							sqlite3_step(stmt);
							sqlite3_finalize(stmt);
							break;
						case PATCHERROR:
						case MAKEERROR:
							sqlite3_prepare_v2(db,
									"UPDATE tests SET status = ?, donetime = unixepoch() WHERE id = ?;",
									-1, &stmt, NULL);
							sqlite3_bind_int(stmt, 1, buf[0]);
							sqlite3_bind_int(stmt, 2, connection->id);
							sqlite3_step(stmt);
							sqlite3_finalize(stmt);
							break;
						}
						connection->status = AWAITING;
						break;
					default:
						fprintf(stderr, "error: bad connection status\n");
						exit(1);
					}
				}
			}
		}
	}
}
