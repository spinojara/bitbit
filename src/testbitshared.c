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

#include "testbitshared.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <openssl/ssl.h>

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int sendall(SSL *ssl, char *buf, size_t len) {
	size_t sent = 0;
	size_t s = 0;

	while (sent < len) {
		s = 0;
		SSL_write_ex(ssl, buf + sent, len - sent, &s);
		if (s <= 0) {
			fprintf(stderr, "send error\n");
			break;
		}
		sent += s;
	}

	return s <= 0;
}

int recvexact(SSL *ssl, char *buf, size_t len) {
	size_t readd = 0;
	size_t s = 0;

	while (readd < len) {
		s = 0;
		SSL_read_ex(ssl, buf + readd, len - readd, &s);
		if (s <= 0) {
			fprintf(stderr, "recv error\n");
			break;
		}
		readd += s;
	}

	return s <= 0;
}

int sendfile(SSL *ssl, int filefd) {
	char buf[BUFSIZ];
	ssize_t n;
	while ((n = read(filefd, buf, sizeof(buf))) > 0)
		if (sendall(ssl, buf, n))
			return 1;
	sendall(ssl, "\0", 1);
	return 0;
}

void SSL_close(SSL *ssl) {
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

char *getpassword(char *password) {
	struct termios old, new;
	tcgetattr(STDIN_FILENO, &old);
	new = old;
	new.c_lflag &= ~ECHO;
	new.c_lflag |= ECHONL;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &new) == -1) {
		fprintf(stderr, "error: tcsetattr\n");
		exit(1);
	}

	printf("password: ");
	fgets(password, 128, stdin);

	tcsetattr(STDIN_FILENO, TCSADRAIN, &old);
	char *p = strchr(password, '\n');
	if (p) {
		*p = '\0';
	}
	else {
		int c;
		/* Clear rest of password from stdin. */
		while ((c = getchar()) != '\n' && c != EOF);
	}

	return password;
}
