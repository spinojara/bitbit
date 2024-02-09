/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TESTBITSHARED_H
#define TESTBITSHARED_H

#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>

enum {
	TESTQUEUE,
	TESTRUNNING,
	TESTDONE,
	BRANCHERROR,
	COMMITERROR,
	PATCHERROR,
	MAKEERROR,
	RUNERROR,
	TESTCANCEL,
};

enum {
	TESTHYPOTHESIS,
	TESTELO,
};

enum {
	CLIENT,
	NODE,
	UPDATE,
	LOG,
};

enum {
	PASSWORD,
	AWAITING,
	RUNNING,
	CANCELLED,
};

void *get_in_addr(struct sockaddr *sa);

int sendall(SSL *ssl, const char *buf, size_t len);

int recvexact(SSL *ssl, char *buf, size_t len);

int sendfile(SSL *ssl, int filefd);

void SSL_close(SSL *ssl);

char *getpassword(char *password);

#endif
