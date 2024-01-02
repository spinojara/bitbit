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
 * GNU General Public License for more details.  *
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
#include <fcntl.h>

#include "testbitshared.h"

int main(int argc, char **argv) {
	char type = CLIENT;
	char *hostname = NULL;
	char *port = "2718";
	char *path = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			i++;
			if (!(i < argc))
				break;
			port = argv[i];
		}
		else if (!strcmp(argv[i], "--log")) {
			type = LOG;
		}
		else if (hostname) {
			path = argv[i];
		}
		else {
			hostname = argv[i];
		}
	}

	if (!hostname || (type == CLIENT && !path)) {
		fprintf(stderr, "usage: testbit hostname [filename | --log]\n");
		return 1;
	}

	int sockfd;
	struct addrinfo hints = { 0 }, *servinfo, *p;
	int rv;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo))) {
		fprintf(stderr, "error: %s\n", gai_strerror(rv));
		return 1;
	}
	
	for (p = servinfo; p; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen)) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (!p) {
		fprintf(stderr, "error: failed to connect to %s\n", hostname);
		return 2;
	}

	freeaddrinfo(servinfo);


	/* Send information and verify password. */
	sendall(sockfd, &type, 1);

	if (type == CLIENT) {
		int filefd = open(path, O_RDONLY, 0);
		if (filefd == -1) {
			fprintf(stderr, "error: failed to open file \"%s\"\n", path);
			close(sockfd);
			return 1;
		}
	
		char password[128] = { 0 };
		char salt[32];
		recvexact(sockfd, salt, sizeof(salt));
		getpassword(password);
		hashpassword(password, salt);
		sendall(sockfd, password, 64);

		/* Architecture dependent. */
		double elo[2] = { 0.0, 10.0 };
		sendall(sockfd, (char *)elo, 16);

		sendfile(sockfd, filefd);

		close(filefd);
	}

	char buf[BUFSIZ] = { 0 };
	int n;
	while ((n = recv(sockfd, buf, sizeof(buf) - 1, 0)) > 0) {
		buf[n] = '\0';
		printf("%s", buf);
		memset(buf, 0, sizeof(buf));
	}

	close(sockfd);
}
