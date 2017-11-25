#include "helloBye_C.hpp"
#include "IPv4_5TupleL2Ident.hpp"
#include "bufArray.hpp"
#include "headers.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

#include <rte_mbuf.h>

extern "C" {

/*
 * Server
 */

void *HelloByeServer_init() {

	auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye::HelloByeServer::Terminate);
	obj->registerStartStateID(HelloBye::HelloByeServer::Hello,
		HelloBye::HelloByeServerHello<IPv4_5TupleL2Ident<mbuf>, mbuf>::factory);

	obj->registerFunction(HelloBye::HelloByeServer::Hello,
		HelloBye::HelloByeServerHello<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye::HelloByeServer::Bye,
		HelloBye::HelloByeServerBye<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);

	return obj;
};

void *HelloByeServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloByeServer_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void HelloByeServer_free(void *obj) {
	delete (reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj));
};

/*
 * Client
 */

void *HelloByeClient_init() {
	auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye::HelloByeClient::Terminate);

	obj->registerFunction(HelloBye::HelloByeClient::Hello,
		HelloBye::HelloByeClientHello<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye::HelloByeClient::Bye,
		HelloBye::HelloByeClientBye<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye::HelloByeClient::RecvBye,
		HelloBye::HelloByeClientRecvBye<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);

	return obj;
};

void HelloByeClient_config(uint32_t srcIP, uint16_t dstPort) {

	HelloBye::HelloByeClientConfig::createInstance();
	HelloBye::HelloByeClientConfig *config = HelloBye::HelloByeClientConfig::getInstance();
	config->setSrcIP(srcIP);
	config->setDstPort(dstPort);
};

void *HelloByeClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort) {

	auto *sm = reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj);

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	HelloBye::HelloByeClientConfig *config = HelloBye::HelloByeClientConfig::getInstance();

	IPv4_5TupleL2Ident<mbuf>::ConnectionID cID;
	cID.dstIP = htonl(config->getSrcIP());
	cID.srcIP = htonl(dstIP);
	cID.dstPort = htons(srcPort);
	cID.srcPort = htons(config->getDstPort());
	cID.proto = Headers::IPv4::PROTO_UDP;

	auto *hello =
		new HelloBye::HelloByeClientHello<IPv4_5TupleL2Ident<mbuf>, mbuf>(dstIP, srcPort);

	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State state(
		HelloBye::HelloByeClient::Hello, hello);

	sm->addState(cID, state, *inPktsBA);

	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloByeClient_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void *HelloByeClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeClient_init()
 */
void HelloByeClient_free(void *obj) {
	delete (reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj));
};
};
