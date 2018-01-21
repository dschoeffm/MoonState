
/*
 * Large parts of this file were copied from the following repo:
 * https://github.com/chris-wood/dtls-test
 *
 * The BIO part is strongly inspired by the following file:
 * https://gist.github.com/darrenjs/4645f115d10aa4b5cebf57483ec82eca
 *
 *
 * Use this command to create the certificate:
 * openssl req -x509 -newkey rsa:2048 -days 3650 -nodes -keyout server-key.pem
 * -out server-cert.pem
 */

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <openssl/conf.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

typedef unsigned int uint;

using namespace std;

struct conn {
	SSL *ssl;
	BIO *wbio;
	BIO *rbio;
};

void dtls_Begin() {
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
}

static int _ssl_verify_peer(int ok, X509_STORE_CTX *ctx) {
	(void)ok;
	(void)ctx;
	return 1;
}

static int _ssl_is_down(SSL *ssl) {
	if (SSL_get_shutdown(ssl) != 0) {
		if (SSL_shutdown(ssl) == 1) {
			return 1;
		}
	}
	return 0;
}

static int _createSocket() {
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

	return sock;
}

int dtls_InitContextFromKeystore(SSL_CTX **ctx) {
	int result = 0;

	// Create a new context using DTLS
	*ctx = SSL_CTX_new(DTLS_method());
	if (ctx == NULL) {
		printf("Error: cannot create SSL_CTX.\n");
		ERR_print_errors_fp(stderr);
		return -1;
	}

	// Set our supported ciphers
	result = SSL_CTX_set_cipher_list(*ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	if (result != 1) {
		printf("Error: cannot set the cipher list.\n");
		ERR_print_errors_fp(stderr);
		return -2;
	}

	// Accept every cert
	SSL_CTX_set_verify(*ctx, SSL_VERIFY_PEER, _ssl_verify_peer);

	return 0;
}

conn *createNewConn(SSL_CTX *ctx) {
	SSL *ssl = SSL_new(ctx);
	if (ssl == NULL) {
		cout << "SSL_new() failed" << endl;
		exit(EXIT_FAILURE);
	}

	BIO *wbio = BIO_new(BIO_s_memQ());
	if (wbio == NULL) {
		cout << "BIO_new() failed" << endl;
		exit(EXIT_FAILURE);
	}

	BIO *rbio = BIO_new(BIO_s_memQ());
	if (rbio == NULL) {
		cout << "BIO_new() failed" << endl;
		exit(EXIT_FAILURE);
	}

	SSL_set_connect_state(ssl);
	SSL_set_bio(ssl, rbio, wbio);

	conn *c = new conn();

	c->ssl = ssl;
	c->wbio = wbio;
	c->rbio = rbio;

	return c;
}

void senderThread(conn *conn) {
	cout << "DEBUG: sender thread started" << endl;
	while (!_ssl_is_down(conn->ssl)) {
		string str;
		cin >> str;

		cout << "DEBUG: writing string to SSL object" << endl;
		unsigned int writeLen = SSL_write(conn->ssl, str.c_str(), strlen(str.c_str()));
		if (writeLen < strlen(str.c_str())) {
			cout << "SSL_write didn't perform as expected" << endl;
		}
	}
};

void handlerThread(int fd, struct sockaddr_in server, conn *conn) {
	char buf[2048];
	int bufLen = 2048;

	cout << "DEBUG: handler thread started" << endl;

	while (!_ssl_is_down(conn->ssl)) {

		// Write out all data the ssl engine wants to write
		int readCount;
		while ((readCount = BIO_read(conn->wbio, buf, bufLen)) > 0) {
			cout << "DEBUG: sending data" << endl;
			int sendBytes = sendto(fd, buf, readCount, 0, (struct sockaddr *)&server,
				sizeof(struct sockaddr_in));
			if (sendBytes != readCount) {
				perror("sendto() failed");
				exit(EXIT_FAILURE);
			}
		}

		// recvfrom is the usual thing you would want to do
		// in this case there only is one peer who might want to
		// communicate with us
		cout << "DEBUG: waiting to receive data" << endl;
		int recvBytes = recv(fd, (void *)buf, bufLen, 0);
		// recv might just time out
		/*
		if (recvBytes == -1) {
			perror("recv failed()");
			exit(EXIT_FAILURE);
		}
		*/

		if (recvBytes > 0) {
			if (BIO_write(conn->rbio, buf, recvBytes) != recvBytes) {
				cout << "BIO_write() failed" << endl;
				exit(EXIT_FAILURE);
			}

			if (SSL_is_init_finished(conn->ssl)) {
				// Here the handshake is already finished

				// Try to read incoming byte from the connection
				int readLen = SSL_read(conn->ssl, buf, bufLen);
				if ((readLen <= 0) && (SSL_get_shutdown(conn->ssl) == 0)) {
					cout << "SSL_read() failed" << endl;
					cout << "shutdown: " << SSL_get_shutdown(conn->ssl) << endl;
					exit(EXIT_FAILURE);
				}

				// Print whatever the server sent us
				cout << "Server: " << reinterpret_cast<char *>(buf) << endl;
			} else {
				// We are currently conducting a handshake

				// The return code is pretty useless, throw it away
				SSL_connect(conn->ssl);
			}
		}

		// Write out all data the ssl engine wants to write
		while ((readCount = BIO_read(conn->wbio, buf, bufLen)) > 0) {
			cout << "DEBUG sending data" << endl;
			int sendBytes = sendto(fd, buf, readCount, 0, (struct sockaddr *)&server,
				sizeof(struct sockaddr_in));
			if (sendBytes != readCount) {
				perror("sendto() failed");
				exit(EXIT_FAILURE);
			}
		}
	}
};

void usage(string name) {
	cout << "Usage: " << name << "<server IP> <server port>" << endl;
	exit(0);
}

int main(int argc, char **argv) {

	if (argc < 3) {
		usage(string(argv[0]));
	}

	// Prepare server address
	struct sockaddr_in server;
	if (inet_aton(argv[1], &server.sin_addr) != 1) {
		cout << "inet_aton() failed" << endl;
		exit(EXIT_FAILURE);
	}
	server.sin_port = htons(atoi(argv[2]));
	server.sin_family = AF_INET;

	// Initialize whatever OpenSSL state is necessary to execute the DTLS protocol.
	dtls_Begin();

	SSL_CTX *ctx;

	if (dtls_InitContextFromKeystore(&ctx) < 0) {
		exit(EXIT_FAILURE);
	}

	// Create the client UDP socket
	int sock = _createSocket();

	// Create a new connection
	conn *conn = createNewConn(ctx);

	// Start the handshake
	SSL_connect(conn->ssl);

	thread sender(senderThread, conn);
	thread handler(handlerThread, sock, server, conn);

	sender.join();
	handler.join();

	close(sock);
}
