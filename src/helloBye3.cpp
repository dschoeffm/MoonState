#ifndef HELLOBYE3PROTO_CPP
#define HELLOBYE3PROTO_CPP

#include <cstdlib>
#include <sstream>

#include "IPv4_5TupleL2Ident.hpp"
#include "headers.hpp"
#include "helloBye3.hpp"
#include "mbuf.hpp"
#include "samplePacket.hpp"

// using namespace Headers;
// using namespace std;

namespace HelloBye3 {

namespace Server {

void *factory(Identifier<mbuf>::ConnectionID id) {
	(void)id;
	struct server *s = new server();
	s->serverCookie = rand() % 256;
	s->clientCookie = 0;
	return s;
};

void runHello(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface) {

	server *s = reinterpret_cast<struct server *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	DEBUG_ENABLED(std::cout << "HelloBye3::Server::runHello() pkt: " << (void *)pkt->getData()
							<< ", ident: " << msg->ident << std::endl;)

	if (msg->role != 0) {
		//		std::abort();
		std::cout << "HelloBye3::Server::runHello() client hello wrong - role not client"
				  << std::endl;
		funIface.transition(States::Terminate);
		delete (s);
		funIface.freePkt();
		return;
	}

	if (msg->msg != 0) {
		//		std::abort();
		std::cout << "HelloBye3::Server::runHello() client hello wrong - msg not hello"
				  << std::endl;
		funIface.transition(States::Terminate);
		delete (s);
		funIface.freePkt();
		return;
	}

	// Get client cookie
	s->clientCookie = msg->cookie;
	DEBUG_ENABLED(std::cout << "HelloBye3::Server::runHello() clientCookie: "
							<< static_cast<int>(s->clientCookie) << std::endl;)

	msg->role = msg::ROLE_SERVER;
	msg->msg = msg::MSG_HELLO;
	msg->cookie = s->serverCookie;

	// Set the IP header stuff
	// Leave the payload length alone for now...

	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best

	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	funIface.transition(States::Bye);
};

void runBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface) {

	server *s = reinterpret_cast<struct server *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_CLIENT) || (msg->msg != msg::MSG_BYE)) {
		std::cout << "HelloBye3::Server::runBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		delete (s);
		funIface.freePkt();
		return;
	}

	if (msg->cookie != s->serverCookie) {
		std::cout << "HelloBye3::Server::runBye() Client sent over wrong cookie" << std::endl;
		funIface.transition(States::Terminate);
		delete (s);
		funIface.freePkt();
		return;
	}

	// Prepare new packet
	msg->cookie = s->clientCookie;
	msg->role = msg::ROLE_SERVER;
	msg->msg = msg::MSG_BYE;

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	// We are done after this -> transition to Terminate
	funIface.transition(States::Terminate);
	delete (s);
};
}; // namespace Server

namespace Client {
StateMachine<Identifier<mbuf>, mbuf>::State createHello(
	uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint64_t ident) {

	struct client *c = new client();
	c->srcIp = srcIp;
	c->dstIp = dstIp;
	c->srcPort = srcPort;
	c->dstPort = dstPort;
	c->ident = ident;
	c->clientCookie = rand() % 256;
	c->serverCookie = 0;

	StateMachine<Identifier<mbuf>, mbuf>::State state(
		States::Hello, reinterpret_cast<void *>(c));
	return state;
};

void runHello(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface) {

	client *c = reinterpret_cast<struct client *>(state.stateData);

	memset(pkt->getData(), 0, pkt->getDataLen());

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->setVersion();
	ipv4->setIHL(5);
	ipv4->ttl = 64;
	ipv4->setLength(100 - 14);
	ipv4->setDstIP(c->dstIp);
	ipv4->setSrcIP(c->srcIp);
	ipv4->setProtoUDP();
	ipv4->checksum = 0;

	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());
	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	// Prepare msg
	msg->ident = c->ident;
	msg->role = msg::ROLE_CLIENT;
	msg->cookie = c->clientCookie;
	msg->msg = msg::MSG_HELLO;

	ether->ethertype = htons(0x0800);

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	udp->setDstPort(c->dstPort);
	udp->setSrcPort(c->srcPort);
	udp->setPayloadLength(50);

	funIface.transition(States::Bye);
};

void runBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface) {

	client *c = reinterpret_cast<struct client *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	DEBUG_ENABLED(std::cout << "HelloBye3::Client::runBye() function called, ident:"
							<< msg->ident << std::endl;)

	if ((msg->role != msg::ROLE_SERVER) || (msg->msg != msg::MSG_HELLO)) {
		std::cout << "HelloBye3::Client::runBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		delete (c);
		funIface.freePkt();
		return;
	}

	// Get server cookie
	c->serverCookie = msg->cookie;

	msg->cookie = c->serverCookie;
	msg->role = msg::ROLE_CLIENT;
	msg->msg = msg::MSG_BYE;

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	DEBUG_ENABLED(
		std::cout << "HelloBye3::Client::runBye() Dump of outgoing packet" << std::endl;)
	DEBUG_ENABLED(hexdump(pkt->getData(), 64);)

	// We need to wait for the server reply
	funIface.transition(States::RecvBye);
};

void runRecvBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface) {

	client *c = reinterpret_cast<struct client *>(state.stateData);

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_SERVER) || (msg->msg != msg::MSG_BYE)) {
		std::cout << "HelloBye3::Client::runRecvBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		delete (c);
		funIface.freePkt();
		return;
	}

	if (msg->cookie != c->clientCookie) {
		std::cout << "HelloBye3::Client::runRecvBye() Server sent over wrong cookie"
				  << std::endl;
		std::cout << "Expected: " << static_cast<int>(c->clientCookie);
		std::cout << ", Got: " << static_cast<int>(msg->cookie) << std::endl;
		funIface.transition(States::Terminate);
		delete (c);
		funIface.freePkt();
		return;
	}

	funIface.freePkt();

	// We are done after this -> transition to Terminate
	funIface.transition(States::Terminate);
	delete (c);
};
}; // namespace Client
}; // namespace HelloBye3

extern "C" {

/*
 * Server
 */

void *HelloBye3_Server_init() {

	srand(time(NULL));

	auto *obj = new StateMachine<HelloBye3::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye3::Server::States::Terminate);
	obj->registerStartStateID(HelloBye3::Server::States::Hello, HelloBye3::Server::factory);

	obj->registerFunction(HelloBye3::Server::States::Hello, HelloBye3::Server::runHello);
	obj->registerFunction(HelloBye3::Server::States::Bye, HelloBye3::Server::runBye);

	return obj;
};

void *HelloBye3_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<HelloBye3::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3_Server_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void HelloBye3_Server_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye3::Identifier<mbuf>, mbuf> *>(obj));
};

/*
 * Client
 */

void *HelloBye3_Client_init() {

	srand(time(NULL));

	auto *obj = new StateMachine<HelloBye3::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye3::Client::States::Terminate);

	obj->registerFunction(HelloBye3::Client::States::Hello, HelloBye3::Client::runHello);
	obj->registerFunction(HelloBye3::Client::States::Bye, HelloBye3::Client::runBye);
	obj->registerFunction(HelloBye3::Client::States::RecvBye, HelloBye3::Client::runRecvBye);

	return obj;
};

void *HelloBye3_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint32_t dstIP,
	uint16_t srcPort, uint16_t dstPort, uint64_t ident) {

	auto *sm = reinterpret_cast<StateMachine<HelloBye3::Identifier<mbuf>, mbuf> *>(obj);

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	HelloBye3::Identifier<mbuf>::ConnectionID cID;
	cID.ident = ident;

	auto state = HelloBye3::Client::createHello(srcIP, dstIP, srcPort, dstPort, ident);

	sm->addState(cID, state, inPktsBA);

	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void *HelloBye3_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount) {
	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm = reinterpret_cast<StateMachine<HelloBye3::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3_Client_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye3::Identifier<mbuf>, mbuf> *>(obj));
};
};
#endif /* HELLOBYE3PROTO_CPP */
