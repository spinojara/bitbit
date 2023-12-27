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

#include "testbitshared.h"
#include "sprt.h"

int main(int argc, char **argv) {
	char *hostname = NULL;
	char *port = "2718";

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			i++;
			if (!(i < argc))
				break;
			port = argv[i];
		}
		else {
			hostname = argv[i];
		}
	}

	if (!hostname)
		fprintf(stderr, "usage: testbitn hostname\n");

	int sockfd;
	struct addrinfo hints = { 0 }, *servinfo, *p;
	int rv;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo))) {
		fprintf(stderr, "error: %s\n", gai_strerror(rv));
		return 1;
	}

	for (p = servinfo; p; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			fprintf(stderr, "error: connect\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen)) {
			close(sockfd);
			fprintf(stderr, "error: connect\n");
			continue;
		}

		break;
	}

	if (!p) {
		fprintf(stderr, "error: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	/* Send information and verify password. */
	char password[128] = { 0 };
	char salt[32];
	recv(sockfd, salt, sizeof(salt), 0);
	getpassword(password);
	hashpassword(password, salt);
	sendall(sockfd, password, strlen(password));

	char buf[BUFSIZ];

	while (recv(sockfd, buf, sizeof(buf), 0) > 0) {
		printf("%s\n", buf);
		/* Start test based on data. */
	}

	return 0;
}
