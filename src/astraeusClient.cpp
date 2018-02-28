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

	if (mp != nullptr) {
		sm.registerGetPktCB([=]() {
			mbuf *buf = reinterpret_cast<mbuf *>(rte_pktmbuf_alloc(mp));
			assert(buf != nullptr);
			assert(buf->buf_addr != nullptr);
			return buf;
		});
	}

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

void Astraeus_Client::initHandshakeNoTransition(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *pkt) {

	astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);

	int sendLen = AstraeusProto::generateInitGivenHandleAndNonce(client->handle,
		reinterpret_cast<uint8_t *>(pkt->getData()) + sizeof(Headers::Ethernet) +
			sizeof(Headers::IPv4) + sizeof(Headers::Udp),
		nonce);
	sodium_increment(nonce, sizeof(nonce));

	pkt->setDataLen(
		sendLen + +sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp));

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
};

void Astraeus_Client::initHandshake(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {

	Astraeus_Client::initHandshakeNoTransition(state, pkt);

	funIface.setTimeout(
		std::chrono::milliseconds(5), [](SM::State &state, SM::FunIface &funIface) {
			funIface.transition(States::DELETED);
			astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);
			delete (client);
		});

	funIface.transition(States::HANDSHAKE);
};

void Astraeus_Client::runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state,
	mbuf *pkt, StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface) {

	astraeusClient *client = reinterpret_cast<astraeusClient *>(state.stateData);
	int sendLen;
	handleHandshakeClient(client->handle,
		reinterpret_cast<uint8_t *>(pkt->getData()) + sizeof(Headers::Ethernet) +
			sizeof(Headers::IPv4) + sizeof(Headers::Udp),
		sendLen);

	pkt->setDataLen(
		sendLen + +sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Udp));

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

void AstraeusClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	uint32_t srcIP, uint16_t srcPort) {

	try {
		auto config = reinterpret_cast<astraeus_C_config *>(obj);

		for (unsigned int i = 0; i < inCount; i++) {
			IPv4_5TupleL2Ident<mbuf>::ConnectionID cID;
			cID.dstIP = htonl(srcIP);
			cID.srcIP = htonl(config->dstIP);
			cID.dstPort = htons(srcPort + i);
			cID.srcPort = htons(config->dstPort);
			cID.proto = Headers::IPv4::PROTO_UDP;

			auto state = Astraeus_Client::createStateData(
				config->ident, srcIP, config->dstIP, srcPort, config->dstPort);

			config->sm->addStateSinglePacket(cID, state, reinterpret_cast<mbuf *>(inPkts[i]));
		}
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

		config->sm->runPktBatch(inPktsBA);
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
