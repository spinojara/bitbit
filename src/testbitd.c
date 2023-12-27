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

#include "testbitshared.h"
#include "util.h"

#define BACKLOG 10

enum {
	LISTENER,
	PASSWORD,
	TYPE,
	SETUP,
	PATCH,
	AWAITING,
	RUNNING,
};

struct connection {
	char buf[128];
	int status;
};

struct queue {
	char name[BUFSIZ];

	struct queue *next;
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
	
	(*pdfs)[*fd_count].fd = newfd;
	(*pdfs)[*fd_count].events = POLLIN;
	memset(&(*connections)[*fd_count], 0, sizeof(**connections));
	(*connections)[*fd_count].status = PASSWORD;
	++*fd_count;
	return &(*connections)[*fd_count - 1];
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
	char buf[BUFSIZ] = { 0 };
	char filename[128];
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
	connections[0].status = LISTENER;
	fd_count = 1;

	while (1) {
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
					if (newfd == -1) {
						fprintf(stderr, "error: accept\n");
					}
					else {
						struct connection *newconnection;
						newconnection = add_to_pdfs(&pdfs, &connections, newfd, &fd_count, &fd_size);

						char salt[32];
						getrandom(salt, sizeof(salt), 0);
						memcpy(newconnection->buf, password, 128);
						hashpassword(newconnection->buf, salt);
						sendall(newfd, salt, 32);

						/* Hashed password takes up exactly 64 characters.
						 * We can save the salt a little later in the string.
						 */
						memcpy(&newconnection[96], salt, 32);
#if 0
#endif
#if 0
						int n;

						memset(buf, 0, sizeof(buf));
						n = recv(newfd, buf, 128, 0);
						buf[64] = '\0';
						if (n <= 0 || strcmp(newpassword, buf)) {
							sendall(newfd, "Permission denied\n", strlen("Permission denied\n"));
							close(newfd);
							continue;
						}

						memset(buf, 0, sizeof(buf));
						n = recv(newfd, buf, 1, 0);
						if (n <= 0 || (buf[0] != 'c' && buf[0] != 'n')) {
							sendall(newfd, "Bad type\n", strlen("Bad type\n"));
							close(newfd);
							continue;
						}
						if (buf[0] == 'n') {
							continue;
						}


						if (mkdir(buf, 0755) == -1) {
							sendall(newfd, "Failed to create test directory ", strlen("Failed to create test directory "));
							sendall(newfd, buf, strlen(buf));
							sendall(newfd, "\n", strlen("\n"));
							close(newfd);
							continue;
						}
#endif
#if 0
						memset(buf, 0, sizeof(buf));
						n = recv(newfd, buf, 128, 0);
						if (n <= 0) {
							close(pdfs[i].fd);
							del_from_pdfs(pdfs, connections, i, &fd_count);
							continue;
						}
#endif
					}
				}
				else {
					struct connection *connection = &connections[i];
					int fd = pdfs[i].fd;

					time_t t;
					memset(buf, 0, sizeof(buf));
					switch (connection->status) {
					case PASSWORD:
						n = recv(fd, buf, 64, 0);
						if (n <= 0 || strcmp(connection->buf, buf)) {
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						connection->status = TYPE;
						/* Now create a name based of the sha256
						 * of the current time and salt.
						 */
						t = time(NULL);
						printf("%s", ctime_r(&t, buf));
						size_t len = strlen(buf);
						/* The salt is still saved at character 96. */
						memcpy(buf + len, &connection->buf[96], 32);
						uint32_t hash[8];
						sha256(buf, len + 32, hash);
						sprintf(connection->buf,
								"/var/lib/testbit/%08x%08x%08x%08x%08x%08x%08x%08x",
								hash[0], hash[1], hash[2], hash[3],
								hash[4], hash[5], hash[6], hash[7]);
						if (mkdir(connection->buf, 0755) == -1) {
							char *str = "error: cannot create directory \'";
							sendall(fd, str, strlen(str));
							sendall(fd, connection->buf, strlen(connection->buf));
							sendall(fd, "\'\n", 2);
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						break;
					case TYPE:
						n = recv(fd, buf, 1, 0);
						if (n <= 0 || (buf[0] != 'c' && buf[0] != 'n')) {
							char *str = "error: bad type\n";
							sendall(fd, str, strlen(str));
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						connection->status = buf[0] == 'c' ? SETUP : AWAITING;
						break;
					case SETUP:
						/* Architecture dependent. */
						n = recv(fd, buf, 16, 0);
						double elo0 = *(double *)buf;
						double elo1 = *(double *)(&buf[8]);
						printf("%lf, %lf\n", elo0, elo1);
						if (n != 16 || elo0 < -100.0 || 100.0 < elo0 || elo1 < -100.0 || elo1 > 100.0) {
							char *str = "error: bad constants\n";
							sendall(fd, str, strlen(str));
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						connection->status = PATCH;
						memcpy(filename, connection->buf, 128);
						appendstr(filename, "/test");
						FILE *f = fopen(filename, "w");
						t = time(NULL);
						if (!f || fprintf(f, "Start:                %s"
								     "H0:                   Elo <= %lf\n"
								     "H1:                   Elo >= %lf\n"
								     "Status:               Queue\n",
								     ctime_r(&t, buf), elo0, elo1) < 0) {
							char *str = "error: cannot write to file \'";
							sendall(fd, str, strlen(str));
							sendall(fd, filename, strlen(filename));
							sendall(fd, "\'\n", 2);
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}

						fclose(f);
						break;
					case PATCH:
						n = recv(fd, buf, sizeof(buf) - 1, 0);
						if (n <= 0) {
							char *str = "error: bad send\n";
							sendall(fd, str, strlen(str));
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}

						memcpy(filename, connection->buf, 128);
						appendstr(filename, "/patch");
						int filefd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
						if (filefd == -1 || write(filefd, buf, n) == -1) {
							char *str = "error: cannot write to file \'";
							sendall(fd, str, strlen(str));
							sendall(fd, filename, strlen(filename));
							sendall(fd, "\'\n", 2);
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						close(filefd);
						if (buf - strchr(buf, '\0') <= n) {
							char *str = "queued test\n";
							sendall(fd, str, strlen(str));
							del_from_pdfs(pdfs, connections, i, &fd_count);
							close(fd);
							break;
						}
						break;
					case AWAITING:
						/* A node should not send anything while awaiting. */
						del_from_pdfs(pdfs, connections, i, &fd_count);
						close(fd);
						break;
					case RUNNING:
						break;
					default:
						fprintf(stderr, "error: bad connection status\n");
						exit(1);
					}
				}
			}
		}
		/* Loop through queue and start available tests. */
		
	}
	free(pdfs);
	free(connections);
}
