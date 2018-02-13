#include <cstring>
#include <functional>

#include "dtlsClient.hpp"
#include "headers.hpp"

using SM = StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>;

namespace DTLS_Client {

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

	// Do not query the BIO for an MTU
	SSL_CTX_set_options(ctx, SSL_OP_NO_QUERY_MTU);

	return ctx;
};

void configStateMachine(SM &sm) {
	sm.registerFunction(States::DOWN, initHandshake);
	sm.registerFunction(States::HANDSHAKE, runHandshake);
	sm.registerFunction(States::ESTABLISHED, sendData);
	sm.registerFunction(States::RUN_TEARDOWN, runTeardown);

	sm.registerEndStateID(States::DELETED);
};

SM::State createStateData(SSL_CTX *ctx, uint32_t localIP, uint32_t remoteIP,
	uint16_t localPort, uint16_t remotePort) {

	// Create necessary structures
	dtlsClient *client = new dtlsClient();
	memset(client, 0, sizeof(dtlsClient));
	SM::State state(DTLS_Client::States::DOWN, reinterpret_cast<void *>(client));

	// Set the trivial stuff
	client->counter = 0;
	client->localIP = localIP;
	client->remoteIP = remoteIP;
	client->localPort = localPort;
	client->remotePort = remotePort;

	// Create SSL and memQ BIOs
	client->ssl = SSL_new(ctx);
	assert(client->ssl != NULL);

	client->wbio = BIO_new(BIO_s_memQ());
	assert(client->wbio != NULL);

	client->rbio = BIO_new(BIO_s_memQ());
	assert(client->rbio != NULL);

	// Make sure openSSL knows, it is a client
	SSL_set_connect_state(client->ssl);
	SSL_set_bio(client->ssl, client->rbio, client->wbio);

	// Set the MTU manually, 1280 is too short, but it should always work
	SSL_set_mtu(client->ssl, 1280);

	return state;
}

static int writeAllDataAvailable(dtlsClient *client, mbuf *pkt, SM::FunIface &funIface) {

	int allHeaderLength =
		sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp);

	int ret = 0;

	// Is there data to write? (There should be)
	if (BIO_eof(client->wbio) == false) {

		DEBUG_ENABLED(
			std::cout << "DTLS_Client::writeAllDataAvailable() sending packet" << std::endl;)

		Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		int udpMaxLen = pkt->getBufLen() - allHeaderLength;

		// Try to read bytes openssl wants to write
		int dataLen = BIO_read(client->wbio, udp->getPayload(), udpMaxLen);
		assert(dataLen > 0);
		pkt->setDataLen(dataLen + allHeaderLength);

		// Set the whole Header stuff
		ethernet->setEthertype(Headers::Ethernet::ETHERTYPE_IPv4);

		ipv4->setVersion();
		ipv4->setIHL(5);
		ipv4->setPayloadLength(dataLen + sizeof(Headers::Udp));
		ipv4->setProtoUDP();
		ipv4->setSrcIP(client->localIP);
		ipv4->setDstIP(client->remoteIP);
		ipv4->checksum = 0;

		udp->setDstPort(client->remotePort);
		udp->setSrcPort(client->localPort);
		udp->setPayloadLength(dataLen);

		ret++;
	} else {
		funIface.freePkt();
	}

	// Maybe there is more data to send, therefore request extra packets if need be
	while (BIO_eof(client->wbio) == false) {

		DEBUG_ENABLED(std::cout << "DTLS_Client::writeAllDataAvailable() sending extra packet"
								<< std::endl;)

		// Try to get an extra packet
		mbuf *xtraPkt = funIface.getPkt();
		assert(xtraPkt != NULL);

		Headers::Ethernet *ethernet =
			reinterpret_cast<Headers::Ethernet *>(xtraPkt->getData());
		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		int udpMaxLen = xtraPkt->getBufLen() - allHeaderLength;

		// Try to read data from the BIO
		int dataLen = BIO_read(client->wbio, udp->getPayload(), udpMaxLen);
		assert(dataLen > 0);
		xtraPkt->setDataLen(dataLen + allHeaderLength);

		// Set the whole Header stuff
		ethernet->setEthertype(Headers::Ethernet::ETHERTYPE_IPv4);

		ipv4->setVersion();
		ipv4->setIHL(5);
		ipv4->setPayloadLength(dataLen + sizeof(Headers::Udp));
		ipv4->setProtoUDP();
		ipv4->setSrcIP(client->localIP);
		ipv4->setDstIP(client->remoteIP);
		ipv4->checksum = 0;

		udp->setDstPort(client->remotePort);
		udp->setSrcPort(client->localPort);
		udp->setPayloadLength(dataLen);

		ret++;
	}

	return ret;
}

void initHandshake(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	DEBUG_ENABLED(std::cout << "DTLS_Client::initHandshake() is called" << std::endl;)
	dtlsClient *client = reinterpret_cast<dtlsClient *>(state.stateData);

	SSL_connect(client->ssl);

	assert(writeAllDataAvailable(client, pkt, funIface) >= 1);

	funIface.transition(States::HANDSHAKE);
};

void runHandshake(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	DEBUG_ENABLED(std::cout << "DTLS_Client::runHandshake() is called" << std::endl;)

	dtlsClient *client = reinterpret_cast<dtlsClient *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(client->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	SSL_connect(client->ssl);

	writeAllDataAvailable(client, pkt, funIface);

	// Check if the handshake is finished
	if (SSL_is_init_finished(client->ssl)) {
		funIface.transition(States::ESTABLISHED);
		DEBUG_ENABLED(
			std::cout << "DTLS_Client::runHandshake() Connection established" << std::endl;)

		int allHeaderLength =
			sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp);

		// Write out a first packet
		mbuf *xtraPkt = funIface.getPkt();
		assert(xtraPkt != NULL);

		Headers::Ethernet *ethernet =
			reinterpret_cast<Headers::Ethernet *>(xtraPkt->getData());
		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		// Set some payload, and hope the buffer is not tiny...
		char payload[] = "FIRST PACKET";
		strcpy(reinterpret_cast<char *>(udp->getPayload()), payload);
		int dataLen = strlen(reinterpret_cast<char *>(udp->getPayload())) + 1;
		assert(dataLen > 0);
		xtraPkt->setDataLen(dataLen + allHeaderLength);

		// Set the whole Header stuff
		ethernet->setEthertype(Headers::Ethernet::ETHERTYPE_IPv4);

		ipv4->setVersion();
		ipv4->setIHL(5);
		ipv4->setPayloadLength(dataLen + sizeof(Headers::Udp));
		ipv4->setProtoUDP();
		ipv4->setSrcIP(client->localIP);
		ipv4->setDstIP(client->remoteIP);
		ipv4->checksum = 0;

		udp->setDstPort(client->remotePort);
		udp->setSrcPort(client->localPort);
		udp->setPayloadLength(dataLen);
	}
};

void sendData(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	dtlsClient *client = reinterpret_cast<dtlsClient *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(client->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	char buf[20];

	// Try to read from the DTLS connection
	int readLen = SSL_read(client->ssl, buf, 20);
	assert(readLen > 0);

	// Compare what we got to what we expect
	assert(strcmp(buf, "FIRST PACKET") == 0);

	// Start to shutdown the connection
	SSL_shutdown(client->ssl);

	// Same procedure as everytime
	writeAllDataAvailable(client, pkt, funIface);
};

void runTeardown(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	DEBUG_ENABLED(std::cout << "DTLS_Client::runTeardown() is called" << std::endl;)

	dtlsClient *client = reinterpret_cast<dtlsClient *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(client->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	if (SSL_shutdown(client->ssl) == 1) {
		// In this case, we need to free the object
		SSL_free(client->ssl);
		delete (client);

		funIface.freePkt();
	}

	// Same procedure as everytime
	writeAllDataAvailable(client, pkt, funIface);
};

}; // namespace DTLS_Client

extern "C" {

// The user should never look at this...
struct Dtls_C_config {
	uint32_t dstIP;
	uint16_t dstPort;
	std::array<uint8_t, 6> srcMac;
	std::array<uint8_t, 6> dstMac;
	SSL_CTX *ctx;
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *sm;
};

void *DtlsClient_init(uint32_t dstIP, uint16_t dstPort) {
	try {
		auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

		DTLS_Client::configStateMachine(*obj);

		struct Dtls_C_config *ret = new Dtls_C_config();
		ret->dstIP = dstIP;
		ret->dstPort = dstPort;
		ret->ctx = DTLS_Client::createCTX();
		ret->sm = obj;

		return ret;
	} catch (std::exception *e) {
		std::cout << "DtlsClient_init() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *DtlsClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint16_t srcPort) {

	try {
		auto config = reinterpret_cast<Dtls_C_config *>(obj);

		BufArray<mbuf> *inPktsBA =
			new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

		IPv4_5TupleL2Ident<mbuf>::ConnectionID cID;
		cID.dstIP = htonl(srcIP);
		cID.srcIP = htonl(config->dstIP);
		cID.dstPort = htons(srcPort);
		cID.srcPort = htons(config->dstPort);
		cID.proto = Headers::IPv4::PROTO_UDP;

		auto state = DTLS_Client::createStateData(
			config->ctx, srcIP, config->dstIP, srcPort, config->dstPort);

		config->sm->addState(cID, state, *inPktsBA);

		*sendCount = inPktsBA->getSendCount();
		*freeCount = inPktsBA->getFreeCount();

		return inPktsBA;

	} catch (std::exception *e) {
		std::cout << "DtlsClient_connect() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void DtlsClient_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	try {
		BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

		inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
		inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

		delete (inPktsBA);

	} catch (std::exception *e) {
		std::cout << "DtlsClient_getPkts() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *DtlsClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	try {
		BufArray<mbuf> *inPktsBA =
			new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

		auto config = reinterpret_cast<Dtls_C_config *>(obj);

		config->sm->runPktBatch(*inPktsBA);
		*sendCount = inPktsBA->getSendCount();
		*freeCount = inPktsBA->getFreeCount();

		return inPktsBA;

	} catch (std::exception *e) {
		std::cout << "DtlsClient_process() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void DtlsClient_free(void *obj) {
	try {
		auto config = reinterpret_cast<Dtls_C_config *>(obj);

		delete (config->sm);
		OPENSSL_free(config->ctx);
		delete (config);

	} catch (std::exception *e) {
		std::cout << "DtlsClient_free() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};
};
