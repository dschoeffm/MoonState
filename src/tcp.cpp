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

	/*
	assert(tcp->getSynFlag());
	assert(!tcp->getAckFlag());
	assert(!tcp->getFinFlag());
	assert(!tcp->getRstFlag());
	*/

	// Send SYN ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	uint16_t tmp16 = tcp->getSrcPort();
	tcp->setSrcPort(tcp->getDstPort());
	tcp->setDstPort(tmp16);

	ipv4->ttl = 64;
	ipv4->checksum = 0;

	if ((!tcp->getSynFlag()) || tcp->getAckFlag() || tcp->getFinFlag() || tcp->getRstFlag()) {
		tcp->clearFlags();
		tcp->setRstFlag();

		funIface.transition(States::END);
		return;
	}

	c->seqRemote = tcp->getSeq();
	c->seqLocal = 0;

	tcp->setAck(c->seqRemote + 1);
	//	std::cout << "Setting ack to: " << c->seqRemote +1 << std::endl;
	tcp->setSeq(0);
	//	std::cout << "Setting seq to: " << 0 << std::endl;
	tcp->setAckFlag();
	tcp->setOffset(5);

	funIface.transition(States::fin);
};

void runFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface) {
	connection *c = reinterpret_cast<struct connection *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Tcp *tcp = reinterpret_cast<Headers::Tcp *>(ipv4->getPayload());

	/*
	assert(!tcp->getSynFlag());
	assert(tcp->getAckFlag());
	assert(!tcp->getFinFlag());
	assert(!tcp->getRstFlag());
	*/

	// Send FIN ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	uint16_t tmp16 = tcp->getSrcPort();
	tcp->setSrcPort(tcp->getDstPort());
	tcp->setDstPort(tmp16);

	ipv4->ttl = 64;
	ipv4->checksum = 0;

	/*
	assert(tcp->getAck() == 1);
	assert(tcp->getSeq() == (c->seqRemote + 1));
	*/

	if (tcp->getSynFlag() || (!tcp->getAckFlag()) || tcp->getFinFlag() || tcp->getRstFlag() ||
		(tcp->getAck() != 1) || (tcp->getSeq() != (c->seqRemote + 1))) {
		tcp->clearFlags();
		tcp->setRstFlag();

		funIface.transition(States::END);
		return;
	}

	c->seqLocal = 1;
	c->seqRemote = tcp->getSeq();

	//	tcp->setAck(c->seqRemote + 1);
	tcp->setSeq(c->seqLocal);
	tcp->setFinFlag();
	tcp->setAck(c->seqRemote);
	tcp->setOffset(5);

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

	/*
	assert(!tcp->getSynFlag());
	assert(tcp->getAckFlag());
	assert(tcp->getFinFlag());
	assert(!tcp->getRstFlag());
	*/

	// Send ACK
	uint32_t tmp = ipv4->getSrcIP();
	ipv4->setSrcIP(ipv4->getDstIP());
	ipv4->setDstIP(tmp);

	uint16_t tmp16 = tcp->getSrcPort();
	tcp->setSrcPort(tcp->getDstPort());
	tcp->setDstPort(tmp16);

	ipv4->ttl = 64;
	ipv4->checksum = 0;

	/*
	assert(tcp->getAck() == 2);
	assert(tcp->getSeq() == c->seqRemote);
	*/

	if (tcp->getSynFlag() || (!tcp->getAckFlag()) || (!tcp->getFinFlag()) ||
		tcp->getRstFlag() || (tcp->getAck() != 2) || (tcp->getSeq() != c->seqRemote)) {
		tcp->clearFlags();
		tcp->setRstFlag();

		funIface.transition(States::END);
		return;
	}

	//	std::cout << "tcp->getSeq() = " << tcp->getSeq() << std::endl;
	//	std::cout << "c->seqRemote = " << c->seqRemote << std::endl;
	c->seqLocal++;
	c->seqRemote = tcp->getSeq();

	tcp->setSeq(c->seqLocal);
	tcp->setAck(c->seqRemote + 1);
	tcp->clearFlags();
	tcp->setAckFlag();
	tcp->setOffset(5);

	funIface.transition(States::END);
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

void TCP_Server_free(void *obj) {
	delete (reinterpret_cast<StateMachine<TCP::Identifier, mbuf> *>(obj));
};
};

#endif /* TCP_CPP */
