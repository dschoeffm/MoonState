#include "helloBye_C.hpp"
#include "IPv4_5TupleL2Ident.hpp"
#include "bufArray.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

#include <rte_mbuf.h>

extern "C" {
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

	BufArray<mbuf> *inPktsBA = new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount);

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
};

void HelloByeServer_free(void *obj) {
	delete (reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj));
};
};
