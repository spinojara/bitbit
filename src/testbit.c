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
#include <signal.h>

#include <openssl/ssl.h>

#include "testbitshared.h"
#include "util.h"

int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);
	char type = CLIENT;
	char *hostname = NULL;
	char *port = "2718";
	char *path = NULL;
	char *status = NULL;

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
		else if (!strcmp(argv[i], "--update")) {
			type = UPDATE;
		}
		else if (path) {
			status = argv[i];
		}
		else if (hostname) {
			path = argv[i];
		}
		else {
			hostname = argv[i];
		}
	}

	if (!hostname || (type == CLIENT && !path) || (type == UPDATE &&
				(!path || !status || !strint(path) || (strcmp(status, "cancel") && strcmp(status, "requeue"))))) {
		fprintf(stderr, "usage: testbit hostname [filename | --log | --update id status]\n");
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
		fprintf(stderr, "error: failed to connect to %s\n", hostname);
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

	/* Send information and verify password. */
	sendall(ssl, &type, 1);

	if (type == CLIENT || type == UPDATE) {
		char password[128] = { 0 };
		getpassword(password);
		sendall(ssl, password, 128);

		if (type == CLIENT) {
			int filefd = open(path, O_RDONLY, 0);
			if (filefd == -1) {
				fprintf(stderr, "error: failed to open file \"%s\"\n", path);
				SSL_close(ssl);
				return 1;
			}
	
			/* Architecture dependent. */
			double timecontrol[2] = { 10.0, 0.1 };
			double alphabeta[2] = { 0.05, 0.05 };
			double elo[2] = { 0.0, 4.0 };
			sendall(ssl, (char *)timecontrol, 16);
			sendall(ssl, (char *)alphabeta, 16);
			sendall(ssl, (char *)elo, 16);

			sendfile(ssl, filefd);

			close(filefd);
		}
		else if (type == UPDATE) {
			uint64_t id = strint(path);
			char newstatus = status[0] == 'c' ? TESTCANCEL : TESTQUEUE;
			sendall(ssl, (char *)&id, sizeof(id));
			sendall(ssl, &newstatus, 1);
		}
	}

	char buf[BUFSIZ];
	size_t n;
	while (SSL_read_ex(ssl, buf, sizeof(buf) - 1, &n)) {
		buf[n] = '\0';
		printf("%s", buf);
	}

	SSL_close(ssl);
	SSL_CTX_free(ctx);
	return 0;
}
