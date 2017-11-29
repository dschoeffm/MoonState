#include "helloBye2_C.hpp"
#include "bufArray.hpp"
#include "headers.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

#include <rte_mbuf.h>

extern "C" {

/*
 * Server
 */

void *HelloBye2_Server_init() {

	srand(time(NULL));

	auto *obj = new StateMachine<HelloBye2::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye2::Server::States::Terminate);
	obj->registerStartStateID(HelloBye2::Server::States::Hello,
		HelloBye2::Server::Hello<HelloBye2::Identifier<mbuf>, mbuf>::factory);

	obj->registerFunction(HelloBye2::Server::States::Hello,
		HelloBye2::Server::Hello<HelloBye2::Identifier<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye2::Server::States::Bye,
		HelloBye2::Server::Bye<HelloBye2::Identifier<mbuf>, mbuf>::run);

	return obj;
};

void *HelloBye2_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<HelloBye2::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye2_Server_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void HelloBye2_Server_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye2::Identifier<mbuf>, mbuf> *>(obj));
};

/*
 * Client
 */

void *HelloBye2_Client_init() {

	srand(time(NULL));

	auto *obj = new StateMachine<HelloBye2::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye2::Client::States::Terminate);

	obj->registerFunction(HelloBye2::Client::States::Hello,
		HelloBye2::Client::Hello<HelloBye2::Identifier<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye2::Client::States::Bye,
		HelloBye2::Client::Bye<HelloBye2::Identifier<mbuf>, mbuf>::run);
	obj->registerFunction(HelloBye2::Client::States::RecvBye,
		HelloBye2::Client::RecvBye<HelloBye2::Identifier<mbuf>, mbuf>::run);

	return obj;
};

void HelloBye2_Client_config(uint32_t srcIP, uint16_t dstPort) {

	HelloBye2::HelloBye2ClientConfig::createInstance();
	HelloBye2::HelloBye2ClientConfig *config =
		HelloBye2::HelloBye2ClientConfig::getInstance();
	config->setSrcIP(srcIP);
	config->setDstPort(dstPort);
};

void *HelloBye2_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort,
	uint64_t ident) {

	auto *sm = reinterpret_cast<StateMachine<HelloBye2::Identifier<mbuf>, mbuf> *>(obj);

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	HelloBye2::Identifier<mbuf>::ConnectionID cID;
	cID.ident = ident;

	auto *hello = new HelloBye2::Client::Hello<HelloBye2::Identifier<mbuf>, mbuf>(
		dstIP, srcPort, ident);

	StateMachine<HelloBye2::Identifier<mbuf>, mbuf>::State state(
		HelloBye2::Client::States::Hello, hello);

	sm->addState(cID, state, *inPktsBA);

	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye2_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void *HelloBye2_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<HelloBye2::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeClient_init()
 */
void HelloBye2_Client_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye2::Identifier<mbuf>, mbuf> *>(obj));
};
};
