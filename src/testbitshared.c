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

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int sendall(int fd, char *buf, size_t len) {
	size_t sent = 0;
	ssize_t s = -1;

	while (sent < len) {
		s = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
		if (s <= 0) {
			fprintf(stderr, "send error\n");
			break;
		}
		sent += s;
	}

	return s <= 0;
}

int recvexact(int fd, char *buf, size_t len) {
	size_t readd = 0;
	ssize_t s = -1;

	while (readd < len) {
		s = recv(fd, buf + readd, len - readd, 0);
		if (s <= 0) {
			fprintf(stderr, "recv error\n");
			break;
		}
		readd += s;
	}

	return s <= 0;
}

int sendfile(int fd, int filefd) {
	char buf[BUFSIZ];
	ssize_t n;
	while ((n = read(filefd, buf, sizeof(buf))) > 0)
		if (sendall(fd, buf, n))
			return 1;
	sendall(fd, "\0", 1);
	return 0;
}

static const uint32_t k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rightrotate(uint32_t value, unsigned count) {
	return value >> count | value << (32 - count);
}

void sha256(const char *str, size_t len, uint32_t hash[8]) {
	size_t bytes = ((len / 64) + 1) * 64;
	size_t L = 8 * len;
	size_t chunks = bytes * 8 / 512;

	uint8_t *p = calloc(bytes, 1);
	memcpy(p, str, len);
	p[len] = 0x80;
	p[bytes - 8] = L >> 56;
	p[bytes - 7] = L >> 48;
	p[bytes - 6] = L >> 40;
	p[bytes - 5] = L >> 32;
	p[bytes - 4] = L >> 24;
	p[bytes - 3] = L >> 16;
	p[bytes - 2] = L >>  8;
	p[bytes - 1] = L >>  0;

	hash[0] = 0x6a09e667;
	hash[1] = 0xbb67ae85;
	hash[2] = 0x3c6ef372;
	hash[3] = 0xa54ff53a;
	hash[4] = 0x510e527f;
	hash[5] = 0x9b05688c;
	hash[6] = 0x1f83d9ab;
	hash[7] = 0x5be0cd19;
	for (size_t i = 0; i < chunks; i++) {
		uint32_t w[64];
		for (int j = 0; j < 16; j++)
			w[j] = p[64 * i + 4 * j + 3] | (p[64 * i + 4 * j + 2] << 8) |
				(p[64 * i + 4 * j + 1] << 16) | (p[64 * i + 4 * j + 0] << 24);

		for (int j = 16; j < 64; j++) {
			uint32_t s0 = rightrotate(w[j - 15], 7) ^ rightrotate(w[j - 15], 18) ^ (w[j - 15] >> 3);
			uint32_t s1 = rightrotate(w[j - 2], 17) ^ rightrotate(w[j - 2], 19) ^ (w[j - 2] >> 10);
			w[j] = w[j - 16] + s0 + w[j - 7] + s1;
		}

		uint32_t a = hash[0];
		uint32_t b = hash[1];
		uint32_t c = hash[2];
		uint32_t d = hash[3];
		uint32_t e = hash[4];
		uint32_t f = hash[5];
		uint32_t g = hash[6];
		uint32_t h = hash[7];

		for (int j = 0; j < 64; j++) {
			uint32_t s1 = rightrotate(e, 6) ^ rightrotate(e, 11) ^ rightrotate(e, 25);
			uint32_t ch = (e & f) ^ (~e & g);
			uint32_t temp1 = h + s1 + ch + k[j] + w[j];
			uint32_t s0 = rightrotate(a, 2) ^ rightrotate(a, 13) ^ rightrotate(a, 22);
			uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			uint32_t temp2 = s0 + maj;

			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}

		hash[0] += a;
		hash[1] += b;
		hash[2] += c;
		hash[3] += d;
		hash[4] += e;
		hash[5] += f;
		hash[6] += g;
		hash[7] += h;
	}
	free(p);
}

char *hashpassword(char *password, const char salt[32]) {
	size_t len = strlen(password);
	memcpy(password + len, salt, 32);
	uint32_t hash[8];
	sha256(password, len + 32, hash);
	sprintf(password, "%08x%08x%08x%08x%08x%08x%08x%08x", hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7]);
	return password;
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
	password[63] = '\0';
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
