
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
#include <openssl/conf.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

// Taken from:
// https://stackoverflow.com/a/20602159
struct pairhash {
public:
	template <typename T, typename U> std::size_t operator()(const std::pair<T, U> &x) const {
		return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
	}
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

static int _createSocket(int port) {
	int sock;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}

	if (::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Unable to bind");
		exit(EXIT_FAILURE);
	}

	return sock;
}

int dtls_InitContextFromKeystore(SSL_CTX **ctx, const char *keyname) {
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

	// The client doesn't have to send it's certificate
	SSL_CTX_set_verify(*ctx, SSL_VERIFY_PEER, _ssl_verify_peer);

	// Load key and certificate
	char certfile[1024];
	char keyfile[1024];
	sprintf(certfile, "./%s-cert.pem", keyname);
	sprintf(keyfile, "./%s-key.pem", keyname);

	// Load the certificate file; contains also the public key
	result = SSL_CTX_use_certificate_file(*ctx, certfile, SSL_FILETYPE_PEM);
	if (result != 1) {
		printf("Error: cannot load certificate file.\n");
		printf("IN CASE YOU DIDN'T CREATE ONE:\n");
		printf("openssl req -x509 -newkey rsa:2048 -days 3650 -nodes -keyout server-key.pem "
			   "-out server-cert.pem\n\n");

		ERR_print_errors_fp(stderr);
		return -4;
	}

	// Load private key
	result = SSL_CTX_use_PrivateKey_file(*ctx, keyfile, SSL_FILETYPE_PEM);
	if (result != 1) {
		printf("Error: cannot load private key file.\n");
		ERR_print_errors_fp(stderr);
		return -5;
	}

	// Check if the private key is valid
	result = SSL_CTX_check_private_key(*ctx);
	if (result != 1) {
		printf("Error: checking the private key failed. \n");
		ERR_print_errors_fp(stderr);
		return -6;
	}

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

	SSL_set_accept_state(ssl);
	SSL_set_bio(ssl, rbio, wbio);

	conn *c = new conn();

	c->ssl = ssl;
	c->wbio = wbio;
	c->rbio = rbio;

	return c;
}

int handlePacket(int fd, struct conn *conn, char *buf, int bufLen, int recvBytes,
	struct sockaddr_in *src_addr) {

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

		// Try to write the same bytes to the connection
		if (readLen > 0) {
			int writeLen = SSL_write(conn->ssl, buf, readLen);
			if (writeLen <= 0) {
				cout << "SSL_write() failed" << endl;
				exit(EXIT_FAILURE);
			}
		}
	} else {
		// We are currently conducting a handshake

		// The return code is pretty useless, throw it away
		SSL_accept(conn->ssl);
	}

	// Write out all data
	int readCount;
	while ((readCount = BIO_read(conn->wbio, buf, bufLen)) > 0) {
		int sendBytes = sendto(
			fd, buf, readCount, 0, (struct sockaddr *)src_addr, sizeof(struct sockaddr_in));
		if (sendBytes != readCount) {
			perror("sendto() failed");
			exit(EXIT_FAILURE);
		}
	}

	if (SSL_get_shutdown(conn->ssl) != 0) {
		if (SSL_shutdown(conn->ssl) == 1) {
			SSL_free(conn->ssl);
			delete (conn);
			return 1;
		}
	}

	return 0;
}

int main() {
	// Initialize whatever OpenSSL state is necessary to execute the DTLS protocol.
	dtls_Begin();

	SSL_CTX *ctx;

	if (dtls_InitContextFromKeystore(&ctx, "server") < 0) {
		exit(0);
	}

	// Create the server UDP listener socket
	int sock = _createSocket(4433);

	unordered_map<pair<unsigned long, unsigned short>, struct conn *, pairhash> connections;

	char buf[2048];

	while (true) {
		struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(struct sockaddr_in);

		// Try and recv a packet
		int ret =
			recvfrom(sock, (void *)buf, 2048, 0, (struct sockaddr *)&src_addr, &addrlen);
		if (ret < 0) {
			perror("recvfrom failed");
			exit(EXIT_FAILURE);
		}

		// Look up if there already exists a connection
	conn_lockup:
		auto conn_it = connections.find({src_addr.sin_addr.s_addr, src_addr.sin_port});
		if (conn_it == connections.end()) {
			cout << "Connection from new client" << endl;
			connections.insert(
				{{src_addr.sin_addr.s_addr, src_addr.sin_port}, createNewConn(ctx)});
			goto conn_lockup;
		}

		// Just double checking
		if (conn_it == connections.end()) {
			cout << "conn_it is invalid..." << endl;
			exit(EXIT_FAILURE);
		}

		if (handlePacket(sock, conn_it->second, buf, 2048, ret, &src_addr) == 1) {
			connections.erase(conn_it);
		}
	}

	close(sock);
}
