#ifndef HELLOBYEPROTO_HPP
#define HELLOBYEPROTO_HPP

#include "common.hpp"
#include "stateMachine.hpp"

/*
 * ===================================
 * States
 * ===================================
 *
 */

struct HelloByeServer {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID Terminate = 2;
};

struct HelloByeClient {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID RecvBye = 2;
	static constexpr StateID Terminate = 3;
};

/*
 * ===================================
 * Forward declarations
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeServerHello;
template <class Identifier, class Packet> class HelloByeServerBye;
template <class Identifier, class Packet> class HelloByeClientHello;
template <class Identifier, class Packet> class HelloByeClientBye;
template <class Identifier, class Packet> class HelloByeClientRecvBye;

/*
 * ===================================
 * Client Config
 * ===================================
 *
 */

class HelloByeClientConfig {
private:
	uint32_t srcIp;
	uint16_t dstPort;

	static HelloByeClientConfig *instance;
	HelloByeClientConfig() : srcIp(0), dstPort(0){};

public:
	auto getSrcIP() { return srcIp; }
	auto getDstPort() { return dstPort; }

	void setSrcIP(uint32_t newIP) { srcIp = newIP; }
	void setDstPort(uint16_t newPort) { dstPort = newPort; }

	static void createInstance() { instance = new HelloByeClientConfig; }

	static HelloByeClientConfig *getInstance() { return instance; }
};

/*
 * ===================================
 * Server Hello
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeServerHello {
	using SM = StateMachine<Identifier, Packet>;
	friend class HelloByeServerBye<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeServerHello();

	static void *factory(typename Identifier::ConnectionID id) {
		(void)id;
		return new HelloByeServerHello();
	}

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeServerHello *t = reinterpret_cast<HelloByeServerHello *>(state.stateData);
		t->fun(state, pkt, funIface);

		// Finish transision to other state
		if (state.state == HelloByeServer::Bye) {
			state.stateData = new HelloByeServerBye<Identifier, Packet>(t);
			delete (t);
		} else if (state.state == HelloByeServer::Terminate) {
			delete (t);
		}
	}
};

/*
 * ===================================
 * Server Bye
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeServerBye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeServerBye(const HelloByeServerHello<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeServerBye *t = reinterpret_cast<HelloByeServerBye *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};

/*
 * ===================================
 * Client Hello
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeClientHello {
	using SM = StateMachine<Identifier, Packet>;
	friend class HelloByeClientBye<Identifier, Packet>;

private:
	int clientCookie;
	uint32_t dstIp;
	uint16_t srcPort;

public:
	HelloByeClientHello(uint32_t dstIp, uint16_t srcPort);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeClientHello *t = reinterpret_cast<HelloByeClientHello *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};

/*
 * ===================================
 * Client Bye
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeClientBye {
	using SM = StateMachine<Identifier, Packet>;
	friend class HelloByeClientRecvBye<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeClientBye(const HelloByeClientHello<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeClientBye *t = reinterpret_cast<HelloByeClientBye *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};

/*
 * ===================================
 * Client RecvBye
 * ===================================
 *
 */

template <class Identifier, class Packet> class HelloByeClientRecvBye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeClientRecvBye(const HelloByeClientBye<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeClientRecvBye *t = reinterpret_cast<HelloByeClientRecvBye *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};

#endif /* HELLOBYEPROTO_HPP */
