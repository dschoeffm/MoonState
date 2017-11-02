#ifndef HELLOBYEPROTO_HPP
#define HELLOBYEPROTO_HPP

#include "stateMachine.hpp"

template <class Identifier, class Packet> class HelloByeServerHello {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeServerHello();

	__attribute__((always_inline)) void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeServerHello *t = reinterpret_cast<HelloByeServerHello *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};


template <class Identifier, class Packet> class HelloByeServerBye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeServerBye();

	__attribute__((always_inline)) void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeServerBye *t = reinterpret_cast<HelloByeServerBye *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};


template <class Identifier, class Packet> class HelloByeClientHello {
	using SM = StateMachine<Identifier, Packet>;
	friend class HelloByeServerBye<Identifier, Packet>;

private:
	int clientCookie;

public:
	HelloByeClientHello();

	__attribute__((always_inline)) void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeClientHello *t = reinterpret_cast<HelloByeClientHello *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};


template <class Identifier, class Packet> class HelloByeClientBye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	HelloByeClientBye();
	HelloByeClientBye(const HelloByeClientHello<Identifier,Packet>& h);

	__attribute__((always_inline)) void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		HelloByeClientBye *t = reinterpret_cast<HelloByeClientBye *>(state.stateData);
		t->fun(state, pkt, funIface);
	}
};

#endif /* HELLOBYEPROTO_HPP */
