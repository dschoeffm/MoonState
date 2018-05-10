#include <cstring>
#include <functional>
#include <sstream>
#include <string>

#include <rte_errno.h>

#include <openssl/dh.h>

#include "dtlsServer.hpp"
#include "headers.hpp"
#include "spinlock.hpp"

#include "measure.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

using SM = StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>;

namespace DTLS_Server {

static DH *get_dh2048() {
	static unsigned char dhp_2048[] = {0x88, 0xD8, 0xA0, 0x31, 0x4B, 0x45, 0x4A, 0xA3, 0x0D,
		0x07, 0xD3, 0x6E, 0x68, 0xDA, 0x74, 0xE7, 0xEF, 0x69, 0x2B, 0x9C, 0x61, 0xC1, 0xDE,
		0x36, 0x58, 0x68, 0x85, 0x80, 0x04, 0x2A, 0xD1, 0x93, 0x52, 0xEA, 0xB6, 0x21, 0x9B,
		0x03, 0x59, 0x8E, 0x3A, 0x72, 0x41, 0x5B, 0x85, 0xDC, 0x46, 0xE6, 0x32, 0x2E, 0xF9,
		0x50, 0x20, 0xE3, 0x63, 0xFB, 0x0B, 0x14, 0xE9, 0x72, 0x8E, 0x6D, 0xBD, 0x69, 0xF5,
		0x65, 0x9F, 0x22, 0x65, 0x65, 0x16, 0x12, 0x13, 0x74, 0x46, 0x99, 0x37, 0x5E, 0x3B,
		0x68, 0x20, 0x8C, 0x61, 0x1B, 0xCE, 0xEB, 0xCD, 0xFA, 0x61, 0xEE, 0x7D, 0x52, 0x00,
		0x5F, 0x1C, 0xDC, 0xCE, 0x74, 0xBB, 0x09, 0x2A, 0xA9, 0xEC, 0xD2, 0x0C, 0xE9, 0x7A,
		0x15, 0x31, 0x1E, 0xD6, 0x37, 0x28, 0x11, 0x93, 0x82, 0x64, 0xD0, 0xF2, 0xF9, 0x93,
		0x51, 0xD9, 0xB5, 0x92, 0xEF, 0xB8, 0xAC, 0x01, 0xB7, 0x80, 0xE6, 0xCB, 0xB6, 0x23,
		0x92, 0xAF, 0x58, 0x79, 0xF9, 0xBB, 0x42, 0x7F, 0x42, 0x93, 0xF9, 0x04, 0x9A, 0xC1,
		0x24, 0x7B, 0x6B, 0xF4, 0x4C, 0xE5, 0x19, 0xEF, 0x64, 0x36, 0x0D, 0xCE, 0x6F, 0x77,
		0xF8, 0x65, 0x32, 0xB6, 0x2D, 0xB7, 0xA2, 0xD0, 0x93, 0x07, 0xEC, 0xFB, 0x62, 0xF3,
		0x01, 0xEB, 0x4F, 0x4E, 0x2B, 0x6A, 0x30, 0xF5, 0x09, 0x17, 0x3A, 0x92, 0x1B, 0x0B,
		0xA9, 0x13, 0x6E, 0x76, 0xDB, 0x39, 0x4D, 0x2F, 0x1A, 0x15, 0x27, 0x7A, 0xE8, 0xEE,
		0x6C, 0xFA, 0x91, 0xC6, 0xF0, 0x51, 0x25, 0xA6, 0x5B, 0x77, 0xC9, 0x14, 0xF1, 0x04,
		0xF0, 0x99, 0x30, 0x13, 0x60, 0xAA, 0x4A, 0x73, 0xF2, 0x7B, 0x2D, 0xAA, 0x7F, 0x02,
		0xB5, 0xD4, 0x86, 0xF0, 0x25, 0x7D, 0x80, 0x9E, 0xAF, 0x3A, 0x90, 0x93, 0x03, 0xDC,
		0xA1, 0xB9, 0x76, 0xB3, 0x6B, 0x8C, 0xDC, 0x83, 0xE3};
	static unsigned char dhg_2048[] = {0x02};
	DH *dh = DH_new();
	BIGNUM *dhp_bn, *dhg_bn;

	if (dh == NULL)
		return NULL;
	dhp_bn = BN_bin2bn(dhp_2048, sizeof(dhp_2048), NULL);
	dhg_bn = BN_bin2bn(dhg_2048, sizeof(dhg_2048), NULL);
	if (dhp_bn == NULL || dhg_bn == NULL || !DH_set0_pqg(dh, dhp_bn, NULL, dhg_bn)) {
		DH_free(dh);
		BN_free(dhp_bn);
		BN_free(dhg_bn);
		return NULL;
	}
	return dh;
}

SSL_CTX *createCTX() {
	uint64_t start = read_rdtsc();

	int result = 0;

	// Create a new context using DTLS
	// This should always use the highest possible version
	SSL_CTX *ctx = SSL_CTX_new(DTLS_method());
	if (ctx == nullptr) {
		std::cout << "DTLS_Server::createCTX() SSL_CTX_new(DTLS_method()) failed"
				  << std::endl;
		return nullptr;
	}

	DH *dh = get_dh2048();
	if (1 != SSL_CTX_set_tmp_dh(ctx, dh)) {
		std::cout << "DTLS_Server::createCTX() SSL_CTX_set_tmp_dh failed" << std::endl;
		return nullptr;
	}
	DH_free(dh);

	// Set our supported ciphers
	// result = SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	result = SSL_CTX_set_cipher_list(ctx, "ALL");
	if (result != 1) {
		std::cout << "DTLS_Server::createCTX() SSL_CTX_set_cipher_list() failed" << std::endl;
		return nullptr;
	}

	auto verifyFun = [](int ok, X509_STORE_CTX *ctx) {
		(void)ok;
		(void)ctx;
		return 1;
	};

	// Load key and certificate
	char certfile[1024] = "./server-cert.pem";
	char keyfile[1024] = "./server-key.pem";

	// Load the certificate file; contains also the public key
	result = SSL_CTX_use_certificate_file(ctx, certfile, SSL_FILETYPE_PEM);
	if (result != 1) {
		throw new std::runtime_error(
			"DTLS::createServerCTX() cannot load certificate file.\n"
			"IN CASE YOU DIDN'T CREATE ONE:\n"
			"openssl req -x509 -newkey rsa:2048 -days 3650 -nodes -keyout server-key.pem "
			"-out server-cert.pem\n\n");
	}

	// Load private key
	result = SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM);
	if (result != 1) {
		throw new std::runtime_error("DTLS::createServerCTX() cannot load private key file");
	}

	// Check if the private key is valid
	result = SSL_CTX_check_private_key(ctx);
	if (result != 1) {
		throw new std::runtime_error("DTLS::createServerCTX() private key check failed");
	}

	// Accept every cert
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyFun);

	// Do not query the BIO for an MTU
	SSL_CTX_set_options(ctx, SSL_OP_NO_QUERY_MTU);

	uint64_t stop = read_rdtsc();
	measureData.openssl += stop - start;

	return ctx;
};

static rte_mempool *mp;

void configStateMachine(SM &sm) {
	//	sm.registerFunction(States::DOWN, initHandshake);
	sm.registerFunction(States::HANDSHAKE, runHandshake);
	sm.registerFunction(States::ESTABLISHED, sendData);
	sm.registerFunction(States::RUN_TEARDOWN, runTeardown);

	sm.registerStartStateID(States::HANDSHAKE, factory);

	assert(mp != nullptr);
	sm.registerGetPktCB([=]() {
		mbuf *buf = reinterpret_cast<mbuf *>(rte_pktmbuf_alloc(mp));
		assert(buf != nullptr);
		assert(buf->buf_addr != nullptr);
		return buf;
	});

	sm.registerEndStateID(States::DELETED);
};

static SSL_CTX *ctx;

void *factory(IPv4_5TupleL2Ident<mbuf>::ConnectionID id) {
	(void)id;

	// Create necessary structures
	uint64_t startMem = read_rdtsc();

	dtlsServer *server = new dtlsServer();
	memset(server, 0, sizeof(dtlsServer));
	SM::State state(DTLS_Server::States::HANDSHAKE, reinterpret_cast<void *>(server));

	uint64_t stopMem = read_rdtsc();
	measureData.memory += stopMem - startMem;

	uint64_t start = read_rdtsc();

	// Create SSL and memQ BIOs
	server->ssl = SSL_new(ctx);
	assert(server->ssl != NULL);

	server->wbio = BIO_new(BIO_s_memQ());
	assert(server->wbio != NULL);

	server->rbio = BIO_new(BIO_s_memQ());
	assert(server->rbio != NULL);

	// Make sure openSSL knows, it is a server
	SSL_set_accept_state(server->ssl);
	SSL_set_bio(server->ssl, server->rbio, server->wbio);

	// Set the MTU manually, 1280 is too short, but it should always work
	SSL_set_mtu(server->ssl, 1280);

	uint64_t stop = read_rdtsc();
	measureData.openssl += stop - start;

	return server;
};

static int writeAllDataAvailable(dtlsServer *server, mbuf *pkt, SM::FunIface &funIface) {

	DEBUG_ENABLED(std::cout << std::dec;)

	int allHeaderLength =
		sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp);

	int ret = 0;

	// Is there data to write? (There should be)
	if (BIO_eof(server->wbio) == false) {

		Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());

		ipv4->setVersion();
		ipv4->setIHL(5);

		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		int udpMaxLen =
			std::min(pkt->getBufLen(), static_cast<uint16_t>(1500)) - allHeaderLength;

		// Try to read bytes openssl wants to write
		uint64_t start = read_rdtsc();

		int dataLen = BIO_read(server->wbio, udp->getPayload(), udpMaxLen);

		uint64_t stop = read_rdtsc();
		measureData.openssl += stop - start;

		assert(dataLen > 0);
		pkt->setDataLen(dataLen + allHeaderLength);

		// Set the whole Header stuff
		ethernet->setDstAddr(server->remoteMac);
		ethernet->setSrcAddr(server->localMac);
		ethernet->setEthertype(Headers::Ethernet::ETHERTYPE_IPv4);

		ipv4->setVersion();
		ipv4->setIHL(5);
		ipv4->setPayloadLength(dataLen + sizeof(Headers::Udp));
		ipv4->setProtoUDP();
		ipv4->setSrcIP(server->localIP);
		ipv4->setDstIP(server->remoteIP);
		ipv4->checksum = 0;
		ipv4->ttl = 64;

		udp->setDstPort(server->remotePort);
		udp->setSrcPort(server->localPort);
		udp->setPayloadLength(dataLen);

		DEBUG_ENABLED(std::cout << "DTLS_Server::writeAllDataAvailable() sending packet. "
								   "len: "
								<< pkt->getDataLen() << std::endl;)

		ret++;
	} else {
		funIface.freePkt();
	}

	// Maybe there is more data to send, therefore request extra packets if need be
	while (BIO_eof(server->wbio) == false) {
		DEBUG_ENABLED(std::cout << "DTLS_Server::writeAllDataAvailable() sending extra packet"
								<< std::endl;)

		// Try to get an extra packet
		mbuf *xtraPkt = funIface.getPkt();
		assert(xtraPkt != NULL);

		DEBUG_ENABLED(std::cout << "hexdump of packet before" << std::endl;
					  hexdump(xtraPkt->getData(), 64);)

		Headers::Ethernet *ethernet =
			reinterpret_cast<Headers::Ethernet *>(xtraPkt->getData());
		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());

		ipv4->setVersion();
		ipv4->setIHL(5);

		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		int udpMaxLen =
			std::min(pkt->getBufLen(), static_cast<uint16_t>(1500)) - allHeaderLength;

		uint64_t start = read_rdtsc();

		int dataLen = BIO_read(server->wbio, udp->getPayload(), udpMaxLen);

		uint64_t stop = read_rdtsc();
		measureData.openssl += stop - start;

		assert(dataLen > 0);
		xtraPkt->setDataLen(dataLen + allHeaderLength);

		// Set the whole Header stuff
		ethernet->setEthertype(Headers::Ethernet::ETHERTYPE_IPv4);
		ethernet->setDstAddr(server->remoteMac);
		ethernet->setSrcAddr(server->localMac);

		ipv4->setPayloadLength(dataLen + sizeof(Headers::Udp));
		ipv4->setProtoUDP();
		ipv4->setSrcIP(server->localIP);
		ipv4->setDstIP(server->remoteIP);
		ipv4->checksum = 0;
		ipv4->ttl = 64;

		udp->setDstPort(server->remotePort);
		udp->setSrcPort(server->localPort);

		udp->setPayloadLength(dataLen);

		DEBUG_ENABLED(std::cout << "DTLS_Server::writeAllDataAvailable() sending packet. "
								   "len: "
								<< xtraPkt->getDataLen() << std::endl;)

		ret++;
	}

	return ret;
}

void runHandshake(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	DEBUG_ENABLED(std::cout << "DTLS_Server::runHandshake() is called" << std::endl;)

	dtlsServer *server = reinterpret_cast<dtlsServer *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	server->localIP = ipv4->getDstIP();
	server->remoteIP = ipv4->getSrcIP();

	server->localPort = udp->getDstPort();
	server->remotePort = udp->getSrcPort();

	memcpy(server->localMac.data(), ethernet->destMac.data(), 6);
	memcpy(server->remoteMac.data(), ethernet->srcMac.data(), 6);

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(server->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	uint64_t start = read_rdtsc();

	SSL_accept(server->ssl);

	// Check if the handshake is finished
	if (SSL_is_init_finished(server->ssl)) {
		funIface.transition(States::ESTABLISHED);

		char buf[2048];
		// Try to read from the DTLS connection
		int readLen = SSL_read(server->ssl, buf, 2048);

		if (readLen > 0) {
			// Reflect the data
			if (SSL_write(server->ssl, buf, readLen) != readLen) {
				throw new std::runtime_error("DTLS_Server::sendData() SSL_write() failed");
			}
		}
	}
	uint64_t stop = read_rdtsc();
	measureData.openssl += stop - start;

	writeAllDataAvailable(server, pkt, funIface);
};

void sendData(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	dtlsServer *server = reinterpret_cast<dtlsServer *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	uint64_t start = read_rdtsc();

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(server->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	char buf[2048];

	// Try to read from the DTLS connection
	int readLen = SSL_read(server->ssl, buf, 2048);
	measureData.numBytes += readLen;
	if (readLen > 0) {

		// Reflect the data
		if (SSL_write(server->ssl, buf, readLen) != readLen) {
			throw new std::runtime_error("DTLS_Server::sendData() SSL_write() failed");
		}
	}

	// XXX Let the client shut the connection down
	// Start to shutdown the connection
	// SSL_shutdown(client->ssl);

	if (SSL_get_shutdown(server->ssl) != 0) {
		// Send a shutdown on our end
		SSL_shutdown(server->ssl);
		// funIface.transition(States::RUN_TEARDOWN);
		funIface.transition(States::DELETED);
	}

	uint64_t stop = read_rdtsc();
	measureData.openssl += stop - start;

	// Same procedure as everytime
	writeAllDataAvailable(server, pkt, funIface);
};

void runTeardown(SM::State &state, mbuf *pkt, SM::FunIface &funIface) {
	DEBUG_ENABLED(std::cout << "DTLS_Server::runTeardown() is called" << std::endl;)

	dtlsServer *server = reinterpret_cast<dtlsServer *>(state.stateData);

	Headers::Ethernet *ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	// Write the incoming packet to the BIO
	int writeBytes = BIO_write(server->rbio, udp->getPayload(), udp->getPayloadLength());
	assert(writeBytes > 0);

	if (SSL_shutdown(server->ssl) == 1) {
		// In this case, we need to free the object
		SSL_free(server->ssl);
		delete (server);
		funIface.transition(States::DELETED);

		funIface.freePkt();
	}

	// Same procedure as everytime
	writeAllDataAvailable(server, pkt, funIface);
};

}; // namespace DTLS_Server

extern "C" {

// The user should never look at this...
struct Dtls_C_config {
	// std::array<uint8_t, 6> srcMac;
	// std::array<uint8_t, 6> dstMac;
	SSL_CTX *ctx;
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *sm;
	struct rte_mempool *mp;
};

void *DtlsServer_init(struct rte_mempool *memp) {
	try {
		auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

		struct Dtls_C_config *ret = new Dtls_C_config();
		ret->ctx = DTLS_Server::createCTX();
		DTLS_Server::ctx = ret->ctx;
		ret->sm = obj;

		DTLS_Server::mp = memp;

		if (memp == nullptr) {
			std::cout << "DtlsServer_init() MemPool creation failed:" << std::endl;
			std::cout << rte_strerror(rte_errno) << std::endl;
			std::abort();
		}

		assert(memp != nullptr);
		ret->mp = memp;

		DTLS_Server::configStateMachine(*obj);

		return ret;
	} catch (std::exception *e) {
		std::cout << "DtlsServer_init() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void DtlsServer_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	try {
		BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

		inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
		inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

		delete (inPktsBA);

	} catch (std::exception *e) {
		std::cout << "DtlsServer_getPkts() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *DtlsServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
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
		std::cout << "DtlsServer_process() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void DtlsServer_free(void *obj) {
	try {

		std::cout << "measure: openssl: " << measureData.openssl << std::endl;
		std::cout << "measure: denseMap: " << measureData.denseMap << std::endl;
		std::cout << "measure: tbb: " << measureData.tbb << std::endl;
		std::cout << "measure: siphash: " << measureData.siphash << std::endl;
		std::cout << "measure: memory: " << measureData.memory << std::endl;
		std::cout << "measure: numPkts: " << measureData.numPkts << std::endl;
		std::cout << "measure: numBytes: " << measureData.numBytes << std::endl;

		std::cout << "CSV:";
		std::stringstream sstream;
		sstream << measureData.openssl << "," << measureData.denseMap << ","
				<< measureData.tbb << "," << measureData.siphash << ","
				<< measureData.memory << "," << measureData.numPkts << ","
				<< measureData.numBytes
				<< std::endl;
		std::cout << sstream.str() << std::endl;

		int fd = open("/root/output.csv", O_CREAT | O_RDWR | O_TRUNC);
		write(fd, sstream.str().c_str(), sstream.str().length());
		close(fd);

		auto config = reinterpret_cast<Dtls_C_config *>(obj);

		delete (config->sm);
		OPENSSL_free(config->ctx);
		delete (config);
		rte_mempool_free(config->mp);

	} catch (std::exception *e) {
		std::cout << "DtlsServer_free() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};
};
