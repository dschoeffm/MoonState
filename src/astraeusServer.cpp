#include <rte_errno.h>

#include "astraeusServer.hpp"

using namespace AstraeusProto;
using namespace Astraeus_Server;

using SM = StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>;

static thread_local uint8_t ecdhPub[crypto_kx_PUBLICKEYBYTES];
static thread_local uint8_t ecdhSec[crypto_kx_SECRETKEYBYTES];
static thread_local uint8_t nonce[ASTRAEUSPROTONONCELEN];
static thread_local identityHandle *ident;

identityHandle *Astraeus_Server::createIdentity() {
	identityHandle *ident = new identityHandle();
	generateIdentity(*ident);
	return ident;
};

void *Astraeus_Server::factory(IPv4_5TupleL2Ident<mbuf>::ConnectionID id) {
	astraeusClient *client = new astraeusClient();
	memset(client, 0, sizeof(astraeusClient));

	// Set the trivial stuff
	client->localIP = ntohl(id.dstIP);
	client->remoteIP = ntohl(id.srcIP);
	client->localPort = ntohs(id.dstPort);
	client->remotePort = ntohs(id.srcPort);

	// Generate a handle
	generateHandleGivenKeyAndNonce(*ident, client->handle, ecdhPub, ecdhSec, nonce);

	return client;
};

void Astraeus_Server::configStateMachine(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> &sm, rte_mempool *mp) {
	sm.registerFunction(States::HANDSHAKE, runHandshake);
	sm.registerFunction(States::ESTABLISHED, sendData);
	sm.registerFunction(States::RUN_TEARDOWN, runTeardown);

	if (mp != nullptr) {
		sm.registerGetPktCB([=]() {
			mbuf *buf = reinterpret_cast<mbuf *>(rte_pktmbuf_alloc(mp));
			assert(buf != nullptr);
			assert(buf->buf_addr != nullptr);
			return buf;
		});
	}

	sm.registerEndStateID(States::DELETED);
	sm.registerStartStateID(States::HANDSHAKE, factory);

	crypto_kx_keypair(ecdhPub, ecdhSec);
	memset(nonce, 0, sizeof(nonce));
};

/*
 * The following functions are the state functions, as they should be registered
 * in the StateMachine<>
 */

void Astraeus_Server::runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *pkt, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {

	astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);
	int sendLen;
	handleHandshakeServer(client->handle,
		reinterpret_cast<uint8_t *>(pkt->getData()) + sizeof(Headers::Ethernet) +
			sizeof(Headers::IPv4) + sizeof(Headers::Udp),
		sendLen);

	sodium_increment(nonce, sizeof(nonce));

	pkt->setDataLen(
		sendLen + sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp));

	auto ethernet = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	auto ipv4 = reinterpret_cast<Headers::IPv4 *>(ethernet->getPayload());
	auto udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	ipv4->checksum = 0;
	ipv4->setDstIP(client->remoteIP);
	ipv4->setSrcIP(client->localIP);
	ipv4->setPayloadLength(sizeof(Headers::Udp) + sendLen);

	udp->checksum = 0;
	udp->setDstPort(client->remotePort);
	udp->setSrcPort(client->localPort);
	udp->setPayloadLength(sendLen);

	if (sendLen <= 0) {
		funIface.freePkt();
	}

	if (!handshakeOngoing(client->handle)) {
		DEBUG_ENABLED(
			std::cout << "Astraeus_Server::runHandshake() handshake finished" << std::endl;)
		// This should actually transition to sendData()
		funIface.transition(States::DELETED);
	}
};

void Astraeus_Server::sendData(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {
	(void)state;
	funIface.transition(States::DELETED);
};

void Astraeus_Server::runTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {
	(void)state;
	funIface.transition(States::DELETED);
};

extern "C" {

struct astraeusServer_C_config {
	// uint32_t dstIP;
	// uint16_t dstPort;
	// identityHandle *ident;
	SM *sm;
	struct rte_mempool *mp;
};

void *AstraeusServer_init() {
	try {
		auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

		auto *ret = new astraeusServer_C_config();
		ident = Astraeus_Server::createIdentity();
		ret->sm = obj;

		std::stringstream poolname;
		poolname << "server pool: ";
		poolname << rte_lcore_id();

		/*
		rte_mempool *memp = rte_pktmbuf_pool_create(
			poolname.str().c_str(), 2048, 0, 0, 2048 + RTE_PKTMBUF_HEADROOM, rte_lcore_id());
		*/
		rte_mempool *memp = rte_pktmbuf_pool_create(
			poolname.str().c_str(), 1024, 0, 0, 2048 + RTE_PKTMBUF_HEADROOM, rte_socket_id());

		if (memp == nullptr) {
			std::cout << "AstraeusServer_init() MemPool creation failed:" << std::endl;
			std::cout << rte_strerror(rte_errno) << std::endl;
			std::abort();
		}

		assert(memp != nullptr);
		ret->mp = memp;

		Astraeus_Server::configStateMachine(*obj, memp);

		return ret;
	} catch (std::exception *e) {
		std::cout << "AstraeusServer_init() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void AstraeusServer_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	try {
		BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

		inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
		inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

		delete (inPktsBA);

	} catch (std::exception *e) {
		std::cout << "AstraeusServer_getPkts() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *AstraeusServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	try {
		BufArray<mbuf> *inPktsBA =
			new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

		auto config = reinterpret_cast<astraeusServer_C_config *>(obj);

		config->sm->runPktBatch(*inPktsBA);
		*sendCount = inPktsBA->getSendCount();
		*freeCount = inPktsBA->getFreeCount();

		return inPktsBA;

	} catch (std::exception *e) {
		std::cout << "AstraeusServer_process() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void AstraeusServer_free(void *obj) {
	try {
		auto config = reinterpret_cast<astraeusServer_C_config *>(obj);

		delete (ident);
		delete (config->sm);
		delete (config);
		rte_mempool_free(config->mp);

	} catch (std::exception *e) {
		std::cout << "AstraeusServer_free() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};
};
