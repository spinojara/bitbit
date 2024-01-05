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
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>

#include "testbitshared.h"
#include "sprt.h"
#include "util.h"

int main(int argc, char **argv) {
	char *hostname = NULL;
	char *port = "2718";
	int threads = -1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			i++;
			if (!(i < argc))
				break;
			port = argv[i];
		}
		else if (!hostname) {
			hostname = argv[i];
		}
		else {
			threads = strint(argv[i]);
		}
	}

	if (!hostname || threads <= 0) {
		fprintf(stderr, "usage: testbitn hostname threads\n");
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

	freeaddrinfo(servinfo);

	if (!p) {
		fprintf(stderr, "error: failed to connect\n");
		return 2;
	}

	char type = NODE;
	sendall(sockfd, &type, 1);

	/* Send information and verify password. */
	char password[128] = { 0 };
	char salt[32];
	if (recvexact(sockfd, salt, sizeof(salt))) {
		return 1;
	}
	getpassword(password);
	hashpassword(password, salt);
	sendall(sockfd, password, strlen(password));

	pid_t pid;
	int wstatus;
	char status;
	char buf[BUFSIZ] = { 0 };
	int n;
	while (1) {
		double maintime, increment;
		double alpha, beta;
		double elo0, elo1;
		uint64_t trinomial[3] = { 0 };
		uint64_t pentanomial[5] = { 0 };
		unsigned long games = 50000;
		double llh = 0.0;

		if (chdir("/tmp")) {
			fprintf(stderr, "error: chdir /tmp\n");
			return 1;
		}

		if (recvexact(sockfd, buf, 48)) {
			fprintf(stderr, "error: constants\n");
			return 1;
		}

		maintime = *(double *)buf;
		increment = *(double *)(&buf[8]);
		alpha = *(double *)(&buf[16]);
		beta = *(double *)(&buf[24]);
		elo0 = *(double *)(&buf[32]);
		elo1 = *(double *)(&buf[40]);
		printf("alpha, beta %lf, %lf\n", alpha, beta);

		pid = fork();
		if (pid == -1)
			return 1;

		if (pid == 0) {
			execlp("rm", "rm", "-rf", "/tmp/bitbit", (char *)NULL);
			fprintf(stderr, "error: exec rm\n");
			return 1;
		}

		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: rm\n");
			return 1;
		}

		pid = fork();
		if (pid == -1)
			return 1;

		/* This should never fail. */
		if (pid == 0) {
			execlp("git", "git", "clone",
				"https://github.com/spinosarus123/bitbit.git",
				"--branch", "master",
				"--single-branch",
				"--depth", "1",
				(char *)NULL);
			fprintf(stderr, "error: exec git clone\n");
			return 1;
		}

		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: git clone\n");
			return 1;
		}

		if (chdir("/tmp/bitbit")) {
			fprintf(stderr, "error: chdir /tmp/bitbit\n");
			return 1;
		}

		int fd = open("patch", O_WRONLY | O_CREAT, 0644);
		memset(buf, 0, sizeof(buf));
		while ((n = recv(sockfd, buf, sizeof(buf) - 1, 0))) {
			if (n <= 0) {
				fprintf(stderr, "error: bad recv\n");
				return 1;
			}

			int len = strlen(buf);
			write(fd, buf, len);

			if (len < n)
				break;
			memset(buf, 0, sizeof(buf));
		}
		close(fd);

		pid = fork();
		if (pid == -1)
			return 1;

		/* This should never fail. */
		if (pid == 0) {
			execlp("make", "make", "SIMD=avx2", "bitbit", (char *)NULL);
			fprintf(stderr, "error: exec make\n");
			return 1;
		}

		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: make\n");
			return 1;
		}

		if (rename("bitbit", "bitbitold")) {
			fprintf(stderr, "error: rename\n");
			return 1;
		}

		pid = fork();
		if (pid == -1)
			return 1;

		if (pid == 0) {
			execlp("git", "git", "apply", "patch", (char *)NULL);
			fprintf(stderr, "error: exec git apply\n");
			return 1;
		}

		/* This can fail if there is something wrong with the patch. */
		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: git apply\n");
			status = PATCHERROR;
			sendall(sockfd, &status, 1);
			continue;
		}

		pid = fork();
		if (pid == -1)
			return 1;

		if (pid == 0) {
			execlp("make", "make", "SIMD=avx2", "bitbit", (char *)NULL);
			fprintf(stderr, "error: exec make\n");
			return 1;
		}

		/* This can fail by a compilation error. */
		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: make\n");
			status = MAKEERROR;
			sendall(sockfd, &status, 1);
			continue;
		}

		char H = sprt(games, trinomial, pentanomial, alpha, beta, maintime, increment, elo0, elo1, &llh, threads, sockfd);
		if (H == HCANCEL)
			continue;

		status = TESTDONE;
		if (sendall(sockfd, &status, 1) ||
				sendall(sockfd, (char *)trinomial, 3 * sizeof(*trinomial)) ||
				sendall(sockfd, (char *)pentanomial, 5 * sizeof(*pentanomial)) ||
				sendall(sockfd, (char *)&llh, 8) ||
				sendall(sockfd, &H, 1))
			return 1;
	}

	close(sockfd);
	return 0;
}
