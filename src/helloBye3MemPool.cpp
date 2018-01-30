#ifndef HELLOBYE3PROTO_CPP
#define HELLOBYE3PROTO_CPP

#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <rte_errno.h>
#include <rte_mempool.h>

#include "IPv4_5TupleL2Ident.hpp"
#include "headers.hpp"
#include "helloBye3MemPool.hpp"
#include "mbuf.hpp"
#include "samplePacket.hpp"

// using namespace Headers;
// using namespace std;

namespace HelloBye3MemPool {

namespace Server {

static struct rte_mempool *mempool;
static std::mutex mempoolMtx;

void *factory(Identifier<mbuf>::ConnectionID id) {
	(void)id;
	struct server *s;
	if (rte_mempool_get(mempool, reinterpret_cast<void **>(&s)) != 0) {
		throw std::runtime_error("HelloBye3MemPool::Server::factory() mempool is empty");
	}
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

	DEBUG_ENABLED(
		std::cout << "HelloBye3MemPool::Server::runHello() pkt: " << (void *)pkt->getData()
				  << ", ident: " << msg->ident << std::endl;)

	if (msg->role != 0) {
		//		std::abort();
		std::cout
			<< "HelloBye3MemPool::Server::runHello() client hello wrong - role not client"
			<< std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(s));
		funIface.freePkt();
		return;
	}

	if (msg->msg != 0) {
		//		std::abort();
		std::cout << "HelloBye3MemPool::Server::runHello() client hello wrong - msg not hello"
				  << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(s));
		funIface.freePkt();
		return;
	}

	// Get client cookie
	s->clientCookie = msg->cookie;
	DEBUG_ENABLED(std::cout << "HelloBye3MemPool::Server::runHello() clientCookie: "
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
		std::cout << "HelloBye3MemPool::Server::runBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(s));
		funIface.freePkt();
		return;
	}

	if (msg->cookie != s->serverCookie) {
		std::cout << "HelloBye3MemPool::Server::runBye() Client sent over wrong cookie"
				  << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(s));
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
	rte_mempool_put(mempool, reinterpret_cast<void *>(s));
};
}; // namespace Server

namespace Client {

static struct rte_mempool *mempool;
static std::mutex mempoolMtx;

StateMachine<Identifier<mbuf>, mbuf>::State createHello(
	uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint64_t ident) {

	struct client *c;
	if (rte_mempool_get(mempool, reinterpret_cast<void **>(&c)) != 0) {
		throw std::runtime_error("HelloBye3MemPool::Client::createHello() mempool is empty");
	}

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

	DEBUG_ENABLED(std::cout << "HelloBye3MemPool::Client::runBye() function called, ident:"
							<< msg->ident << std::endl;)

	if ((msg->role != msg::ROLE_SERVER) || (msg->msg != msg::MSG_HELLO)) {
		std::cout << "HelloBye3MemPool::Client::runBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(c));
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

	DEBUG_ENABLED(std::cout << "HelloBye3MemPool::Client::runBye() Dump of outgoing packet"
							<< std::endl;)
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
		std::cout << "HelloBye3MemPool::Client::runRecvBye() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(c));
		funIface.freePkt();
		return;
	}

	if (msg->cookie != c->clientCookie) {
		std::cout << "HelloBye3MemPool::Client::runRecvBye() Server sent over wrong cookie"
				  << std::endl;
		std::cout << "Expected: " << static_cast<int>(c->clientCookie);
		std::cout << ", Got: " << static_cast<int>(msg->cookie) << std::endl;
		funIface.transition(States::Terminate);
		rte_mempool_put(mempool, reinterpret_cast<void *>(c));
		funIface.freePkt();
		return;
	}

	funIface.freePkt();

	// We are done after this -> transition to Terminate
	funIface.transition(States::Terminate);
	rte_mempool_put(mempool, reinterpret_cast<void *>(c));
};
}; // namespace Client
}; // namespace HelloBye3MemPool

extern "C" {

/*
 * Server
 */

void *HelloBye3MemPool_Server_init() {

	srand(time(NULL));

	HelloBye3MemPool::Server::mempoolMtx.lock();
	if (HelloBye3MemPool::Server::mempool == nullptr) {
		HelloBye3MemPool::Server::mempool = rte_mempool_create("Server MemPool", 8192,
			sizeof(HelloBye3MemPool::Server::server), 128, 0, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
		if (HelloBye3MemPool::Server::mempool == nullptr) {
			std::cout << "HelloBye3MemPool_Server_init() mempool creation failed"
					  << std::endl;
			std::cout << "Error: " << rte_strerror(rte_errno) << std::endl;
			std::abort();
		}
	}
	HelloBye3MemPool::Server::mempoolMtx.unlock();

	auto *obj = new StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye3MemPool::Server::States::Terminate);
	obj->registerStartStateID(
		HelloBye3MemPool::Server::States::Hello, HelloBye3MemPool::Server::factory);

	obj->registerFunction(
		HelloBye3MemPool::Server::States::Hello, HelloBye3MemPool::Server::runHello);
	obj->registerFunction(
		HelloBye3MemPool::Server::States::Bye, HelloBye3MemPool::Server::runBye);

	return obj;
};

void *HelloBye3MemPool_Server_process(void *obj, struct rte_mbuf **inPkts,
	unsigned int inCount, unsigned int *sendCount, unsigned int *freeCount) {

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm =
		reinterpret_cast<StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3MemPool_Server_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void HelloBye3MemPool_Server_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf> *>(obj));
	rte_mempool_free(HelloBye3MemPool::Server::mempool);
};

/*
 * Client
 */

void *HelloBye3MemPool_Client_init() {

	srand(time(NULL));

	HelloBye3MemPool::Client::mempoolMtx.lock();
	if (HelloBye3MemPool::Client::mempool == nullptr) {
		HelloBye3MemPool::Client::mempool = rte_mempool_create("Client MemPool", 8192,
			sizeof(HelloBye3MemPool::Client::client), 128, 0, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
		if (HelloBye3MemPool::Client::mempool == nullptr) {
			std::cout << "HelloBye3MemPool_Client_init() mempool creation failed"
					  << std::endl;
			std::cout << "Error: " << rte_strerror(rte_errno) << std::endl;
			std::abort();
		}
	}
	HelloBye3MemPool::Client::mempoolMtx.unlock();

	auto *obj = new StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf>();

	obj->registerEndStateID(HelloBye3MemPool::Client::States::Terminate);

	obj->registerFunction(
		HelloBye3MemPool::Client::States::Hello, HelloBye3MemPool::Client::runHello);
	obj->registerFunction(
		HelloBye3MemPool::Client::States::Bye, HelloBye3MemPool::Client::runBye);
	obj->registerFunction(
		HelloBye3MemPool::Client::States::RecvBye, HelloBye3MemPool::Client::runRecvBye);

	return obj;
};

void *HelloBye3MemPool_Client_connect(void *obj, struct rte_mbuf **inPkts,
	unsigned int inCount, unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP,
	uint32_t dstIP, uint16_t srcPort, uint16_t dstPort, uint64_t ident) {

	auto *sm =
		reinterpret_cast<StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf> *>(obj);

	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	HelloBye3MemPool::Identifier<mbuf>::ConnectionID cID;
	cID.ident = ident;

	auto state = HelloBye3MemPool::Client::createHello(srcIP, dstIP, srcPort, dstPort, ident);

	sm->addState(cID, state, *inPktsBA);

	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3MemPool_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts) {
	BufArray<mbuf> *inPktsBA = reinterpret_cast<BufArray<mbuf> *>(obj);

	inPktsBA->getSendBufs(reinterpret_cast<mbuf **>(sendPkts));
	inPktsBA->getFreeBufs(reinterpret_cast<mbuf **>(freePkts));

	delete (inPktsBA);
};

void *HelloBye3MemPool_Client_process(void *obj, struct rte_mbuf **inPkts,
	unsigned int inCount, unsigned int *sendCount, unsigned int *freeCount) {
	BufArray<mbuf> *inPktsBA =
		new BufArray<mbuf>(reinterpret_cast<mbuf **>(inPkts), inCount, true);

	auto *sm =
		reinterpret_cast<StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf> *>(obj);
	sm->runPktBatch(*inPktsBA);
	*sendCount = inPktsBA->getSendCount();
	*freeCount = inPktsBA->getFreeCount();

	return inPktsBA;
};

void HelloBye3MemPool_Client_free(void *obj) {
	delete (reinterpret_cast<StateMachine<HelloBye3MemPool::Identifier<mbuf>, mbuf> *>(obj));
	rte_mempool_free(HelloBye3MemPool::Client::mempool);
};
};
#endif /* HELLOBYE3PROTO_CPP */
