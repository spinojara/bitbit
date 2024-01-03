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

#ifndef TESTBITSHARED_H
#define TESTBITSHARED_H

#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

enum {
	TESTQUEUE,
	TESTRUNNING,
	TESTDONE,
	PATCHERROR,
	MAKEERROR,
	RUNERROR,
	TESTCANCEL,
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

int sendall(int fd, char *buf, size_t len);

int recvexact(int fd, char *buf, size_t len);

int sendfile(int fd, int filefd);

void sha256(const char *str, size_t len, uint32_t hash[8]);

char *hashpassword(char *password, const char salt[32]);

char *getpassword(char *password);

#endif
