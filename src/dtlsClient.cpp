#include <functional>

#include "dtlsClient.hpp"

using namespace DTLS_Client;
using SM = StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>;

SSL_CTX *createCTX() {

	int result = 0;

	// Create a new context using DTLS
	// This should always use the highest possible version
	SSL_CTX *ctx = SSL_CTX_new(DTLS_method());
	if (ctx == nullptr) {
		std::cout << "DTLS_Client::createCTX() SSL_CTX_new(DTLS_method()) failed"
				  << std::endl;
		return nullptr;
	}

	// Set our supported ciphers
	result = SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	if (result != 1) {
		std::cout << "DTLS_Client::createCTX() SSL_CTX_set_cipher_list() failed" << std::endl;
		return nullptr;
	}

	auto verifyFun = [](int ok, X509_STORE_CTX *ctx) {
		(void)ok;
		(void)ctx;
		return 1;
	};

	// Accept every cert
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyFun);

	return 0;
};

SM::State *createStateData(SSL_CTX *ctx, uint32_t localIP, uint32_t remoteIP,
	uint16_t localPort, uint16_t remotePort, std::array<uint8_t, 6> localMac,
	std::array<uint8_t, 6> remoteMac) {

	// Create necessary structures
	dtlsClient *client = new dtlsClient();
	memset(client, 0, sizeof(dtlsClient));
	SM::State *state =
		new SM::State(DTLS_Client::States::DOWN, reinterpret_cast<void *>(client));

	// Set the trivial stuff
	client->counter = 0;
	client->localIP = localIP;
	client->remoteIP = remoteIP;
	client->localPort = localPort;
	client->remotePort = remotePort;
	client->localMac = localMac;
	client->remoteMac = remoteMac;

	// Create SSL and memQ BIOs
	client->ssl = SSL_new(ctx);
	assert(client->ssl == NULL);

	client->wbio = BIO_new(BIO_s_memQ());
	assert(client->wbio != NULL);

	client->rbio = BIO_new(BIO_s_memQ());
	assert(client->rbio != NULL);

	SSL_set_connect_state(client->ssl);
	SSL_set_bio(client->ssl, client->rbio, client->wbio);

	return state;
}

void initHandshake(SM::State &state, mbuf *, SM::FunIface &funIface);

void runHandshake(SM::State &state, mbuf *, SM::FunIface &funIface);

void sendData(SM::State &state, mbuf *, SM::FunIface &funIface);

void startTeardown(SM::State &state, mbuf *, SM::FunIface &funIface);

void runTeardown(SM::State &state, mbuf *, SM::FunIface &funIface);
