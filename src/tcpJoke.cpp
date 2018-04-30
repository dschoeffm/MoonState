#include <functional>

#include <rte_mempool.h>

#include "tcp.cpp"
#include "tcpConCtlSimple.hpp"
#include "tcpProtoJoke.hpp"

using ProtoJoke = TcpProtoJoke<TcpConCtlSimple>;

using ServerJoke = TCP::Server<ProtoJoke, TcpConCtlSimple>;

extern "C" {

void *TCP_Server_Joke_init(rte_mempool *mp) {
	auto *obj = new StateMachine<TCP::Identifier, mbuf>();

	obj->registerGetPktCB([mp]() { return reinterpret_cast<mbuf *>(rte_pktmbuf_alloc(mp)); });

	obj->registerEndStateID(TCP::States::END);
	obj->registerStartStateID(TCP::States::syn_ack, ServerJoke::factory);

	obj->registerFunction(TCP::States::syn_ack, ServerJoke::runSynAck);
	obj->registerFunction(TCP::States::est, ServerJoke::runEst);
	//	obj->registerFunction(TCP::States::fin, ServerJoke::runFin);
	obj->registerFunction(TCP::States::ack_fin, ServerJoke::runAckFin);

	return obj;
};

void *TCP_Server_Joke_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<TCP::Identifier, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void TCP_Server_Joke_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void TCP_Server_Joke_free(void *obj) {
	delete (reinterpret_cast<StateMachine<TCP::Identifier, mbuf> *>(obj));
};
};
