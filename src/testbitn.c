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
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
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
#include <signal.h>
#include <ftw.h>

#include <openssl/ssl.h>

#include "testbitshared.h"
#include "sprt.h"
#include "util.h"

int rm(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	UNUSED(sb);
	UNUSED(typeflag);
	UNUSED(ftwbuf);
	int r = remove(path);
	if (r)
		perror(path);
	return r;
}

int rmdir_r(const char *path) {
	return nftw(path, rm, 64, FTW_DEPTH | FTW_PHYS);
}

int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);
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

	SSL_CTX *ctx;
	SSL *ssl;
	BIO *bio;
	ctx = SSL_CTX_new(TLS_client_method());
	if (!ctx) {
		fprintf(stderr, "error: failed to create the SSL context\n");
		return 3;
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	if (!SSL_CTX_set_default_verify_paths(ctx)) {
		fprintf(stderr, "error: failed to set the default trusted certificate store\n");
		return 4;
	}

	if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
		fprintf(stderr, "error: failed to set the minimum TLS protocol version\n");
		return 5;
	}
	
	ssl = SSL_new(ctx);
	if (!ssl) {
		fprintf(stderr, "error: failed to create the SSL object\n");
		return 6;
	}

	bio = BIO_new(BIO_s_socket());
	if (!bio) {
		fprintf(stderr, "error: failed to create the BIO object\n");
		return 7;
	}

	BIO_set_fd(bio, sockfd, BIO_CLOSE);
	SSL_set_bio(ssl, bio, bio);

	if (!SSL_set_tlsext_host_name(ssl, hostname)) {
		fprintf(stderr, "error: failed to set the SNI hostname\n");
		return 8;
	}

	if (!SSL_set1_host(ssl, hostname)) {
		fprintf(stderr, "error: failed to set the certificate verification hostname\n");
		return 9;
	}

	if (SSL_connect(ssl) <= 0) {
		fprintf(stderr, "error: handshake failed\n");
		if (SSL_get_verify_result(ssl) != X509_V_OK)
			fprintf(stderr, "error: %s\n",
					X509_verify_cert_error_string(
						SSL_get_verify_result(ssl)));
		return 10;
	}

	char type = NODE;
	sendall(ssl, &type, 1);

	/* Send information and verify password. */
	char password[128];
	getpassword(password);
	sendall(ssl, password, 128);

	pid_t pid;
	int wstatus;
	char status;
	char buf[BUFSIZ] = { 0 };
	while (1) {
		double maintime, increment;
		double alpha, beta;
		double elo0, elo1;
		uint64_t trinomial[3] = { 0 };
		uint64_t pentanomial[5] = { 0 };
		unsigned long games = 50000;
		double llh = 0.0;
		char dtemp[16] = "testbit-XXXXXX";

		if (chdir("/tmp")) {
			fprintf(stderr, "error: chdir /tmp\n");
			return 1;
		}

		if (recvexact(ssl, buf, 48)) {
			fprintf(stderr, "error: constants\n");
			return 1;
		}

		maintime = ((double *)buf)[0];
		increment = ((double *)buf)[1];
		alpha = ((double *)buf)[2];
		beta = ((double *)buf)[3];
		elo0 = ((double *)buf)[4];
		elo1 = ((double *)buf)[5];

		if (!mkdtemp(dtemp)) {
			fprintf(stderr, "error: failed to create temporary directory\n");
			return 1;
		}

		pid = fork();
		if (pid == -1)
			return 1;

		/* This should never fail. */
		if (pid == 0) {
			execlp("git", "git", "clone",
				"https://github.com/Spinojara/bitbit.git",
				"--branch", "master",
				"--single-branch",
				"--depth", "1",
				dtemp,
				(char *)NULL);
			fprintf(stderr, "error: exec git clone\n");
			return 1;
		}

		if (waitpid(pid, &wstatus, 0) == -1 || WEXITSTATUS(wstatus)) {
			fprintf(stderr, "error: git clone\n");
			return 1;
		}

		if (chdir(dtemp)) {
			fprintf(stderr, "error: chdir %s\n", dtemp);
			return 1;
		}

		int fd = open("patch", O_WRONLY | O_CREAT, 0644);
		size_t n;
		while (SSL_read_ex(ssl, buf, sizeof(buf) - 1, &n)) {
			buf[n] = '\0';
			size_t len = strlen(buf);
			write(fd, buf, len);
			if (len < n)
				break;
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
			sendall(ssl, &status, 1);
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
			sendall(ssl, &status, 1);
			continue;
		}

		char H = sprt(games, trinomial, pentanomial, alpha, beta, maintime, increment, elo0, elo1, &llh, threads, ssl);
		if (chdir("/tmp")) {
			fprintf(stderr, "error: chdir /tmp\n");
			return 1;
		}

		if (rmdir_r(dtemp)) {
			fprintf(stderr, "error: failed to remove temporary directory\n");
			return 1;
		}

		if (H == HCANCEL)
			continue;

		status = TESTDONE;
		if (sendall(ssl, &status, 1) ||
				sendall(ssl, (char *)trinomial, 3 * sizeof(*trinomial)) ||
				sendall(ssl, (char *)pentanomial, 5 * sizeof(*pentanomial)) ||
				sendall(ssl, (char *)&llh, 8) ||
				sendall(ssl, &H, 1))
			return 1;
	}

	SSL_close(ssl);
	SSL_CTX_free(ctx);
	return 0;
}
