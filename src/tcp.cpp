#ifndef TCP_CPP
#define TCP_CPP

#include <type_traits>

#include "tcp.hpp"

namespace TCP {

static constexpr unsigned int MSS = 1460;

template <class Proto, class ConCtl>
typename Server<Proto, ConCtl>::connection *Server<Proto, ConCtl>::factory(
	Identifier::ConnectionID id) {

	/*
	static_assert(std::is_base_of<ConCtlBase, ConCtl>::value,
		"ConCtl has to be derived from ConCtlBase");
	static_assert(std::is_base_of<ConCtlBase, ConCtl>::value,
		"ConCtl has to be derived from ConCtlBase");
		*/

	DEBUG_ENABLED(std::cout << "creating new TCP connection" << std::endl;)

	(void)id;
	struct connection *conn = new connection();
	return conn;
};

template <class Proto, class ConCtl>
void Server<Proto, ConCtl>::runSynAck(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
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

	DEBUG_ENABLED(
		std::cout << "TCP::Server::runSynAck() All received flags are correct" << std::endl;)

	c->seqRemote = tcp->getSeq();
	c->seqLocal = 0;

	tcp->setAck(c->seqRemote + 1);
	//	std::cout << "Setting ack to: " << c->seqRemote +1 << std::endl;
	tcp->setSeq(c->seqLocal++);
	//	std::cout << "Setting seq to: " << 0 << std::endl;
	tcp->setAckFlag();
	tcp->setOffset(5);

	// Trick the dataToSend vecot
	// SYN counts as one byte into the seq number
	c->dataToSend.push_back(0);
	c->dataToSendOffset = 1;

	DEBUG_ENABLED(
		std::cout << "TCP::Server::runSynAck() Sending ACK, transition into ESTABLISHED"
				  << std::endl;)

	funIface.transition(States::est);
};

template <class Proto, class ConCtl>
void Server<Proto, ConCtl>::runEst(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface) {
	connection *c = reinterpret_cast<struct connection *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Tcp *tcp = reinterpret_cast<Headers::Tcp *>(ipv4->getPayload());

	bool freePkt = true;

	/*
	assert(!tcp->getSynFlag());
	assert(tcp->getAckFlag());
	assert(!tcp->getFinFlag());
	assert(!tcp->getRstFlag());
	*/

	// Swap addresses
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

	if (tcp->getSynFlag() || (!tcp->getAckFlag()) || tcp->getRstFlag()) {

		DEBUG_ENABLED(std::cout << "TCP::Server::runEst() Some flag or number doesn't add up"
								<< std::endl;)
		DEBUG_ENABLED(std::cout << "Flags " << tcp->getSynFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "SYN = " << tcp->getSynFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "ACK = " << tcp->getAckFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "FIN = " << tcp->getFinFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "RST = " << tcp->getRstFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "Seq # = " << tcp->getSeq() << std::endl;)
		DEBUG_ENABLED(std::cout << "Ack # = " << tcp->getAck() << std::endl;)
		DEBUG_ENABLED(std::cout << "c->seqLocal = " << c->seqLocal << std::endl;)
		DEBUG_ENABLED(std::cout << "c->seqRemote = " << c->seqRemote << std::endl;)

		tcp->clearFlags();
		tcp->setRstFlag();
		tcp->setAck(0);
		tcp->setSeq(c->seqLocal);
		ipv4->setPayloadLength(sizeof(Headers::Tcp));
		pkt->setDataLen(
			sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Tcp));

		funIface.transition(States::END);
		return;
	}

	DEBUG_ENABLED(
		std::cout << "TCP::Server::runEst() All received flags are correct" << std::endl;)

	// Run whatever congestion control magic we have
	c->conCtl.handlePacket(tcp->getSeq(), tcp->getAck());
	uint32_t resetSeq;
	if (c->conCtl.reset(resetSeq)) {
		c->dataToSendOffset = resetSeq - c->dataToSendSeq;
	}
	c->sendWindow = tcp->getWindow();

	// See how long the TCP payload is and set the current remote seq number
	DEBUG_ENABLED(std::cout << "TCP::Server::runEst() old c->seqRemote = " << c->seqRemote
							<< std::endl;)
	uint16_t numBytesReceived = ipv4->getPayloadLength() - tcp->getHeaderLen();
	DEBUG_ENABLED(std::cout << "TCP::Server::runEst() ipv4->getPayloadLength() = "
							<< ipv4->getPayloadLength() << std::endl;)
	DEBUG_ENABLED(std::cout << "TCP::Server::runEst() tcp->getHeaderLen() = "
							<< static_cast<uint16_t>(tcp->getHeaderLen()) << std::endl;)

	c->seqRemote = tcp->getSeq() + numBytesReceived;
	DEBUG_ENABLED(std::cout << "TCP::Server::runEst() numBytesReceived = " << numBytesReceived
							<< std::endl;)
	DEBUG_ENABLED(std::cout << "TCP::Server::runEst() new c->seqRemote = " << c->seqRemote
							<< std::endl;)

	if (tcp->getAck() > c->dataToSendSeq) {
		uint32_t ackBytes = tcp->getAck() - c->dataToSendSeq;

		DEBUG_ENABLED(std::cout << "TCP::Server::runEst() " << ackBytes
								<< " bytes are acknowledged" << std::endl;)

		DEBUG_ENABLED(std::cout << "ackBytes = " << ackBytes << std::endl;)
		DEBUG_ENABLED(
			std::cout << "c->dataToSendOffset = " << c->dataToSendOffset << std::endl;)
		DEBUG_ENABLED(
			std::cout << "c->dataToSend.size() = " << c->dataToSend.size() << std::endl;)
		DEBUG_ENABLED(std::cout << "tcp->getAck() = " << tcp->getAck() << std::endl;)
		DEBUG_ENABLED(std::cout << "c->dataToSendSeq = " << c->dataToSendSeq << std::endl;)

		assert(c->dataToSendOffset >= ackBytes);
		assert(c->dataToSend.size() >= ackBytes);

		memmove(c->dataToSend.data(), c->dataToSend.data() + ackBytes,
			c->dataToSend.size() - ackBytes);
		c->dataToSendOffset -= ackBytes;
		c->dataToSend.resize(c->dataToSend.size() - ackBytes);
		c->dataToSendSeq += ackBytes;

		DEBUG_ENABLED(std::cout << "(after shrinking) c->dataToSend.size() = "
								<< c->dataToSend.size() << std::endl;)
	}

	if (tcp->getFinFlag()) {
		DEBUG_ENABLED(std::cout << "TCP::Server:runEst() Peer sent FIN" << std::endl;)
		c->closeConnectionAfterSending = true;
		c->seqRemote++;
	}

	tcp->clearFlags();
	tcp->setAckFlag();

	tcp->setSeq(c->seqLocal);
	// tcp->setFinFlag();
	tcp->setAck(c->seqRemote);
	tcp->setOffset(5);

	// funIface.transition(States::ack_fin);

	uint16_t dataLen = ipv4->getPayloadLength() - tcp->getHeaderLen();
	if (dataLen > 0) {
		DEBUG_ENABLED(
			std::cout << "TCP::Server:runEst() pumping data into the protocol implementation"
					  << std::endl;)

		freePkt = false;
		TcpIface tcpIface(*c);
		c->proto.handlePacket(
			reinterpret_cast<uint8_t *>(tcp->getPayload()), dataLen, tcpIface);
	}

	if (!c->dataToSend.empty()) {
		uint32_t maxSend = std::min(static_cast<uint32_t>(c->sendWindow),
			static_cast<uint32_t>(c->conCtl.getConWindow()));
		maxSend = std::min(
			maxSend, static_cast<uint32_t>(c->dataToSend.size() - c->dataToSendOffset));

		DEBUG_ENABLED(std::cout << "TCP::Server::runEst() c->sendWindow = " << c->sendWindow
								<< " bytes" << std::endl;)
		DEBUG_ENABLED(std::cout << "TCP::Server::runEst() c->conCtl.getConWindow = "
								<< c->conCtl.getConWindow() << " bytes" << std::endl;)
		DEBUG_ENABLED(
			std::cout << "TCP::Server::runEst() data available = "
					  << static_cast<uint32_t>(c->dataToSend.size() - c->dataToSendOffset)
					  << " bytes" << std::endl;)

		if (maxSend > 0) {

			freePkt = false;

			uint32_t totalSend = 0;

			uint32_t segmentSend = std::min(maxSend, static_cast<uint32_t>(MSS));
			memcpy(
				tcp->getPayload(), c->dataToSend.data() + c->dataToSendOffset, segmentSend);
			ipv4->setPayloadLength(sizeof(Headers::Tcp) + segmentSend);
			tcp->setSeq(c->seqLocal);
			pkt->setDataLen(sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) +
							sizeof(Headers::Tcp) + segmentSend);

			c->seqLocal += segmentSend;
			c->dataToSendOffset += segmentSend;
			totalSend += segmentSend;

			DEBUG_ENABLED(std::cout << "TCP::Server::runEst() Sending segment with "
									<< segmentSend << " bytes" << std::endl;)

			uint16_t idAdd = 1;

			while (totalSend < maxSend) {
				mbuf *extraPkt = funIface.getPkt();
				segmentSend = std::min(maxSend, static_cast<uint32_t>(MSS));

				Headers::Ethernet *etherX =
					reinterpret_cast<Headers::Ethernet *>(pkt->getData());
				Headers::IPv4 *ipv4X =
					reinterpret_cast<Headers::IPv4 *>(etherX->getPayload());
				Headers::Tcp *tcpX = reinterpret_cast<Headers::Tcp *>(ipv4X->getPayload());

				memcpy(extraPkt->getData(), pkt->getData(),
					sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) + sizeof(Headers::Tcp));

				ipv4X->id = htons(ntohs(ipv4X->id) + idAdd);

				tcpX->setSeq(c->seqLocal);

				memcpy(tcpX->getPayload(),
					c->dataToSend.data() + c->dataToSendOffset + totalSend, segmentSend);
				ipv4->setPayloadLength(sizeof(Headers::Tcp) + segmentSend);

				c->seqLocal += segmentSend;
				totalSend += segmentSend;
				c->dataToSendOffset += segmentSend;

				extraPkt->setDataLen(sizeof(Headers::Ethernet) + sizeof(Headers::IPv4) +
									 sizeof(Headers::Tcp) + segmentSend);

				DEBUG_ENABLED(std::cout << "TCP::Server::runEst() Sending segment with "
										<< segmentSend << " bytes" << std::endl;)
			}
		}
	} else {
		DEBUG_ENABLED(std::cout << "TCP::Server:runEst() no data to be sent" << std::endl;)
	}

	if (c->closeConnectionAfterSending && c->dataToSend.empty()) {
		DEBUG_ENABLED(std::cout << "TCP::Server::runEst() Setting FIN" << std::endl;)
		freePkt = false;
		tcp->setFinFlag();
		c->seqLocal++;
		funIface.transition(States::ack_fin);
	}

	if (freePkt) {
		funIface.freePkt();
	}
};

template <class Proto, class ConCtl>
void Server<Proto, ConCtl>::runAckFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
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

	if (tcp->getSynFlag() || (!tcp->getAckFlag()) || tcp->getRstFlag() ||
		(tcp->getAck() != c->seqLocal) || (tcp->getSeq() != c->seqRemote)) {

		DEBUG_ENABLED(
			std::cout << "TCP::Server::runAckFin() Some flag or number doesn't add up"
					  << std::endl;)
		DEBUG_ENABLED(std::cout << "Flags " << tcp->getSynFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "SYN = " << tcp->getSynFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "ACK = " << tcp->getAckFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "FIN = " << tcp->getFinFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "RST = " << tcp->getRstFlag() << std::endl;)
		DEBUG_ENABLED(std::cout << "Seq # = " << tcp->getSeq() << std::endl;)
		DEBUG_ENABLED(std::cout << "Ack # = " << tcp->getAck() << std::endl;)
		DEBUG_ENABLED(std::cout << "c->seqLocal = " << c->seqLocal << std::endl;)
		DEBUG_ENABLED(std::cout << "c->seqRemote = " << c->seqRemote << std::endl;)

		tcp->clearFlags();
		tcp->setRstFlag();

		funIface.transition(States::END);
		return;
	}

	//	std::cout << "tcp->getSeq() = " << tcp->getSeq() << std::endl;
	//	std::cout << "c->seqRemote = " << c->seqRemote << std::endl;
	if (tcp->getFinFlag()) {
		c->seqLocal++;
		c->seqRemote = tcp->getSeq();

		tcp->setSeq(c->seqLocal);
		tcp->setAck(c->seqRemote + 1);
		tcp->clearFlags();
		tcp->setAckFlag();
		tcp->setOffset(5);

		funIface.transition(States::END);
	} else {
		funIface.freePkt();
	}
};

}; // namespace TCP

#if 0
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
#endif

#endif /* TCP_CPP */
