#include "helloBye_C.hpp"
#include "IPv4_5TupleL2Ident.hpp"
#include "bufArray.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

#include <rte_mbuf.h>

extern "C" {
void *HelloByeServer_init() {

	auto *obj = new StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>();

	obj->registerEndStateID(HelloByeServer::Terminate);
	obj->registerStartStateID(
		HelloByeServer::Hello, HelloByeServerHello<IPv4_5TupleL2Ident<mbuf>, mbuf>::factory);

	obj->registerFunction(
		HelloByeServer::Hello, HelloByeServerHello<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);
	obj->registerFunction(
		HelloByeServer::Bye, HelloByeServerBye<IPv4_5TupleL2Ident<mbuf>, mbuf>::run);

	return obj;
};

void HelloByeServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	struct rte_mbuf **sendPkts, unsigned int *sendCount, struct rte_mbuf **freePkts,
	unsigned int *freeCount) {

	BufArray<mbuf> inPktsBA(reinterpret_cast<mbuf **>(inPkts), inCount);
	BufArray<mbuf> sendPktsBA(reinterpret_cast<mbuf **>(sendPkts), 0);
	BufArray<mbuf> freePktsBA(reinterpret_cast<mbuf **>(freePkts), 0);

	auto *sm = reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(inPktsBA, sendPktsBA, freePktsBA);
	*sendCount = sendPktsBA.getNum();
	*freeCount = freePktsBA.getNum();
};

void HelloByeServer_free(void *obj) {
	delete (reinterpret_cast<StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> *>(obj));
};
};
