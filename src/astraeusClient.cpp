#include <rte_errno.h>

#include "astraeusClient.hpp"

using namespace AstraeusProto;
using namespace Astraeus_Client;

using SM = StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>;

static thread_local uint8_t ecdhPub[crypto_kx_PUBLICKEYBYTES];
static thread_local uint8_t ecdhSec[crypto_kx_SECRETKEYBYTES];
static thread_local uint8_t nonce[ASTRAEUSPROTONONCELEN];

identityHandle *Astraeus_Client::createIdentity() {
	identityHandle *ident = new identityHandle();
	generateIdentity(*ident);
	return ident;
};

void Astraeus_Client::configStateMachine(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> &sm, rte_mempool *mp) {
	sm.registerFunction(States::DOWN, initHandshake);
	sm.registerFunction(States::HANDSHAKE, runHandshake);
	sm.registerFunction(States::ESTABLISHED, sendData);
	sm.registerFunction(States::RUN_TEARDOWN, runTeardown);

	assert(mp != nullptr);
	sm.registerGetPktCB([=]() {
		mbuf *buf = reinterpret_cast<mbuf *>(rte_pktmbuf_alloc(mp));
		assert(buf != nullptr);
		assert(buf->buf_addr != nullptr);
		return buf;
	});

	sm.registerEndStateID(States::DELETED);

	crypto_kx_keypair(ecdhPub, ecdhSec);
	memset(nonce, 0, sizeof(nonce));
};

StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State Astraeus_Client::createStateData(
	AstraeusProto::identityHandle *ident, uint32_t localIP, uint32_t remoteIP,
	uint16_t localPort, uint16_t remotePort) {

	// Create necessary structures
	astraeusClient *client = new astraeusClient();
	memset(client, 0, sizeof(astraeusClient));
	SM::State state(States::DOWN, reinterpret_cast<void *>(client));

	// Set the trivial stuff
	client->localIP = localIP;
	client->remoteIP = remoteIP;
	client->localPort = localPort;
	client->remotePort = remotePort;

	// Generate a handle
	generateHandleGivenKey(*ident, client->handle, ecdhPub, ecdhSec);

	return state;
};

/*
 * The following functions are the state functions, as they should be registered
 * in the StateMachine<>
 */

void Astraeus_Client::initHandshake(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {

	astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);

	int sendLen = AstraeusProto::generateInitGivenHandleAndNonce(
		client->handle, reinterpret_cast<uint8_t *>(pkt->getData()), nonce);
	sodium_increment(nonce, sizeof(nonce));

	pkt->setDataLen(sendLen);

	funIface.transition(States::HANDSHAKE);
};

void Astraeus_Client::runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *pkt, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {

	astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);
	int sendCount;
	handleHandshakeClient(
		client->handle, reinterpret_cast<uint8_t *>(pkt->getData()), sendCount);
	if (sendCount <= 0) {
		funIface.freePkt();
	}

	if (!handshakeOngoing(client->handle)) {
		DEBUG_ENABLED(
			std::cout << "Astraeus_Client::runHandshake() handshake finished" << std::endl;)
		// This should actually transition to sendData()
		funIface.transition(States::DELETED);
	}
};

void Astraeus_Client::sendData(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {
	(void)state;
	funIface.transition(States::DELETED);
};

void Astraeus_Client::runTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {
	(void)state;
	funIface.transition(States::DELETED);
};

extern "C" {

struct astraeus_C_config {
	uint32_t dstIP;
	uint16_t dstPort;
	identityHandle *ident;
	SM *sm;
	struct rte_mempool *mp;
};

void *AstraeusClient_init(uint32_t dstIP, uint16_t dstPort) {
	try {
		auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

		struct astraeus_C_config *ret = new astraeus_C_config();
		ret->dstIP = dstIP;
		ret->dstPort = dstPort;
		ret->ident = createIdentity();
		ret->sm = obj;

		std::stringstream poolname;
		poolname << "client pool: ";
		poolname << rte_lcore_id();

		/*
		rte_mempool *memp = rte_pktmbuf_pool_create(
			poolname.str().c_str(), 2048, 0, 0, 2048 + RTE_PKTMBUF_HEADROOM, rte_lcore_id());
		*/
		rte_mempool *memp = rte_pktmbuf_pool_create(
			poolname.str().c_str(), 1024, 0, 0, 2048 + RTE_PKTMBUF_HEADROOM, rte_socket_id());

		if (memp == nullptr) {
			std::cout << "AstraeusClient_init() MemPool creation failed:" << std::endl;
			std::cout << rte_strerror(rte_errno) << std::endl;
			std::abort();
		}

		assert(memp != nullptr);
		ret->mp = memp;

		Astraeus_Client::configStateMachine(*obj, memp);

		return ret;
	} catch (std::exception *e) {
		std::cout << "AstraeusClient_init() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *AstraeusClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint16_t srcPort) {

	try {
		auto config = reinterpret_cast<astraeus_C_config *>(obj);

		BufArray<mbuf> *inPktsBA =
			new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

		IPv4_5TupleL2Ident<mbuf>::ConnectionID cID;
		cID.dstIP = htonl(srcIP);
		cID.srcIP = htonl(config->dstIP);
		cID.dstPort = htons(srcPort);
		cID.srcPort = htons(config->dstPort);
		cID.proto = Headers::IPv4::PROTO_UDP;

		auto state = Astraeus_Client::createStateData(
			config->ident, srcIP, config->dstIP, srcPort, config->dstPort);

		config->sm->addState(cID, state, *inPktsBA);

		*sendCount = inPktsBA->getSendCount();
		*freeCount = inPktsBA->getFreeCount();

		return inPktsBA;

	} catch (std::exception *e) {
		std::cout << "AstraeusClient_connect() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void AstraeusClient_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	try {
		BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

		inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
		inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

		delete (inPktsBA);

	} catch (std::exception *e) {
		std::cout << "AstraeusClient_getPkts() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void *AstraeusClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	try {
		BufArray<mbuf> *inPktsBA =
			new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

		auto config = reinterpret_cast<astraeus_C_config *>(obj);

		config->sm->runPktBatch(*inPktsBA);
		*sendCount = inPktsBA->getSendCount();
		*freeCount = inPktsBA->getFreeCount();

		return inPktsBA;

	} catch (std::exception *e) {
		std::cout << "AstraeusClient_process() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};

void AstraeusClient_free(void *obj) {
	try {
		auto config = reinterpret_cast<astraeus_C_config *>(obj);

		delete (config->sm);
		delete (config->ident);
		delete (config);
		rte_mempool_free(config->mp);

	} catch (std::exception *e) {
		std::cout << "AstraeusClient_free() caught exception:" << std::endl
				  << e->what() << std::endl;
		std::abort();
	}
};
}
