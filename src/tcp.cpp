#ifndef TCP_CPP
#define TCP_CPP

#include "tcp.hpp"

namespace TCP {

namespace Server {
void *factory(Identifier::ConnectionID id) {
	(void)id;
	struct connection *conn = new connection();
	return conn;
};

void runSynAck(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface) {
	connection *c = reinterpret_cast<struct connection *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Tcp *tcp = reinterpret_cast<Headers::Tcp *>(ipv4->getPayload());

	assert(tcp->getSynFlag());
	assert(!tcp->getAckFlag());
	assert(!tcp->getFinFlag());
	assert(!tcp->getRstFlag());

	// Send SYN ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	ipv4->ttl = 64;

	c->seqRemote = tcp->getAck();
	c->seqLocal = 0;

	tcp->setAck(c->seqRemote + 1);
	tcp->setSeq(0);
	tcp->setAckFlag();

	funIface.transition(States::fin);
};

void runFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface) {
	connection *c = reinterpret_cast<struct connection *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Tcp *tcp = reinterpret_cast<Headers::Tcp *>(ipv4->getPayload());

	assert(!tcp->getSynFlag());
	assert(tcp->getAckFlag());
	assert(!tcp->getFinFlag());
	assert(!tcp->getRstFlag());

	// Send FIN ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	ipv4->ttl = 64;

	assert(tcp->getAck() == 1);
	assert(tcp->getSeq() == (c->seqRemote + 1));
	c->seqLocal = 1;
	c->seqRemote = tcp->getSeq();

	//	tcp->setAck(c->seqRemote + 1);
	tcp->setSeq(c->seqLocal);
	tcp->setFinFlag();
	tcp->setAck(c->seqRemote + 1);

	funIface.transition(States::ack_fin);
};

void runAckFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface) {
	// Parse FIN ACK
	connection *c = reinterpret_cast<struct connection *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Tcp *tcp = reinterpret_cast<Headers::Tcp *>(ipv4->getPayload());

	assert(!tcp->getSynFlag());
	assert(tcp->getAckFlag());
	assert(tcp->getFinFlag());
	assert(!tcp->getRstFlag());

	// Send ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	ipv4->ttl = 64;

	assert(tcp->getAck() == 2);
	assert(tcp->getSeq() == (c->seqRemote + 1));
	c->seqLocal++;
	c->seqRemote = tcp->getSeq();

	//	tcp->setAck(c->seqRemote + 1);
	tcp->setSeq(c->seqLocal);
	tcp->setAck(c->seqRemote + 1);

	funIface.transition(States::ack_fin);
};

}; // namespace Server

}; // namespace TCP

extern "C" {

/*
 * Server
 */

void *TCP_Server_init() {
	auto *obj = new StateMachine<TCP::Identifier, mbuf>();

	obj->registerEndStateID(TCP::Server::States::END);
	obj->registerStartStateID(TCP::Server::States::syn_ack, TCP::Server::factory);

	obj->registerFunction(TCP::Server::States::syn_ack, TCP::Server::runSynAck);
	obj->registerFunction(TCP::Server::States::fin, TCP::Server::runFin);
	obj->registerFunction(TCP::Server::States::ack_fin, TCP::Server::runAckFin);

	return obj;
};

void *TCP_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<TCP::Identifier, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void TCP_Server_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void HelloBye3_Server_free(void *obj) {
	delete (reinterpret_cast<StateMachine<TCP::Identifier, mbuf> *>(obj));
};
};

#endif /* TCP_CPP */
