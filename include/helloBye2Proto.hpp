#ifndef HELLOBYE2PROTO_HPP
#define HELLOBYE2PROTO_HPP

#include <mutex>
#include <sstream>

#include "common.hpp"
#include "exceptions.hpp"
#include "headers.hpp"
#include "stateMachine.hpp"

namespace HelloBye2 {

struct __attribute__((packed)) msg {
	uint64_t ident;
	uint8_t role;
	uint8_t msg;
	uint8_t cookie;

	static constexpr uint8_t ROLE_CLIENT = 0;
	static constexpr uint8_t ROLE_SERVER = 1;

	static constexpr uint8_t MSG_HELLO = 0;
	static constexpr uint8_t MSG_BYE = 1;
};

/*
 * ===================================
 * Identifier
 * ===================================
 *
 */

template <typename Packet> class Identifier {
public:
	class ConnectionID {
	public:
		uint64_t ident;

		bool operator==(const ConnectionID &c) const { return ident == c.ident; };
		bool operator<(const ConnectionID &c) const { return ident < c.ident; };
		operator std::string() const {
			std::stringstream sstream;
			sstream << ident;
			return sstream.str();
		};
		ConnectionID(const ConnectionID &c) : ident(c.ident){};
		ConnectionID() : ident(0){};
		ConnectionID(uint64_t i) : ident(i){};
	};
	struct Hasher {
		uint64_t operator()(const ConnectionID &c) const { return c.ident; };
	};
	static ConnectionID identify(Packet *pkt) {
		Headers::Ethernet *eth = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
		if (eth->getEthertype() != Headers::Ethernet::ETHERTYPE_IPv4) {
			throw new PacketNotIdentified();
		}

		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(eth->getPayload());
		if (ipv4->proto != Headers::IPv4::PROTO_UDP) {
			throw new PacketNotIdentified();
		}

		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());
		return msg->ident;
	};

	static ConnectionID getDelKey() {
		ConnectionID id;
		id.ident = std::numeric_limits<uint64_t>::max();
		return id;
	};

	static ConnectionID getEmptyKey() {
		ConnectionID id;
		id.ident = std::numeric_limits<uint64_t>::max() - 1;
		return id;
	};
};

/*
 * ===================================
 * Forward declarations
 * ===================================
 *
 */

namespace Server {
template <class Identifier, class Packet> class Hello;
template <class Identifier, class Packet> class Bye;
}; // namespace Server

namespace Client {
template <class Identifier, class Packet> class Hello;
template <class Identifier, class Packet> class Bye;
template <class Identifier, class Packet> class RecvBye;
} // namespace Client

/*
 * ===================================
 * Client Config
 * ===================================
 *
 */

/*
class HelloBye2ClientConfig {
private:
	uint32_t srcIp;
	uint16_t dstPort;

	static HelloBye2ClientConfig *instance;
	static std::mutex mtx;
	HelloBye2ClientConfig() : srcIp(0), dstPort(0){};

public:
	auto getSrcIP() { return srcIp; }
	auto getDstPort() { return dstPort; }

	void setSrcIP(uint32_t newIP) { srcIp = newIP; }
	void setDstPort(uint16_t newPort) { dstPort = newPort; }

	static void createInstance() {
		std::lock_guard<std::mutex> lock(mtx);
		if (instance == nullptr) {
			instance = new HelloBye2ClientConfig;
		}
	}

	static HelloBye2ClientConfig *getInstance() { return instance; }
};
*/

class HelloBye2ClientConfig {
private:
	uint32_t srcIp;
	uint16_t dstPort;

public:
	static HelloBye2ClientConfig &getInstance() {
		static HelloBye2ClientConfig instance;
		return instance;
	}

	HelloBye2ClientConfig(){};
	HelloBye2ClientConfig(HelloBye2ClientConfig const &) = delete;
	void operator=(HelloBye2ClientConfig const &) = delete;

	auto getSrcIP() { return srcIp; }
	auto getDstPort() { return dstPort; }

	void setSrcIP(uint32_t newIP) { srcIp = newIP; }
	void setDstPort(uint16_t newPort) { dstPort = newPort; }
};

/*
 * ===================================
 * Server Hello
 * ===================================
 *
 */

namespace Server {

struct States {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID Terminate = 2;
};

template <class Identifier, class Packet> class Hello {
	using SM = StateMachine<Identifier, Packet>;
	friend class Bye<Identifier, Packet>;

private:
	uint8_t clientCookie;
	uint8_t serverCookie;

public:
	Hello();

	static void *factory(typename Identifier::ConnectionID id) {
		(void)id;
		return new Hello();
	}

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		Hello *t = reinterpret_cast<Hello *>(state.stateData);
		t->fun(state, pkt, funIface);

		// Finish transision to other state
		if (state.state == States::Bye) {
			state.stateData = new Bye<Identifier, Packet>(t);
			delete (t);
		} else if (state.state == States::Terminate) {
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

template <class Identifier, class Packet> class Bye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	Bye(const Hello<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		Bye *t = reinterpret_cast<Bye *>(state.stateData);
		t->fun(state, pkt, funIface);
		if (state.state == States::Terminate) {
			delete (t);
		}
	}
};

}; // namespace Server

namespace Client {

struct States {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID RecvBye = 2;
	static constexpr StateID Terminate = 3;
};

/*
 * ===================================
 * Client Hello
 * ===================================
 *
 */

template <class Identifier, class Packet> class Hello {
	using SM = StateMachine<Identifier, Packet>;
	friend class Bye<Identifier, Packet>;

private:
	int clientCookie;
	uint32_t dstIp;
	uint16_t srcPort;
	uint64_t ident;

public:
	Hello(uint32_t dstIp, uint16_t srcPort, uint64_t ident);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		Hello *t = reinterpret_cast<Hello *>(state.stateData);
		t->fun(state, pkt, funIface);

		// Finish transision to other state
		if (state.state == States::Bye) {
			state.stateData = new Bye<Identifier, Packet>(t);
			delete (t);
		} else if (state.state == States::Terminate) {
			delete (t);
		}
	}
};

/*
 * ===================================
 * Client Bye
 * ===================================
 *
 */

template <class Identifier, class Packet> class Bye {
	using SM = StateMachine<Identifier, Packet>;
	friend class RecvBye<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	Bye(const Hello<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		Bye *t = reinterpret_cast<Bye *>(state.stateData);
		t->fun(state, pkt, funIface);

		// Finish transision to other state
		if (state.state == States::RecvBye) {
			state.stateData = new RecvBye<Identifier, Packet>(t);
			delete (t);
		} else if (state.state == States::Terminate) {
			delete (t);
		}
	}
};

/*
 * ===================================
 * Client RecvBye
 * ===================================
 *
 */

template <class Identifier, class Packet> class RecvBye {
	using SM = StateMachine<Identifier, Packet>;

private:
	int clientCookie;
	int serverCookie;

public:
	RecvBye(const Bye<Identifier, Packet> *in);

	PROD_INLINE void fun(
		typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface);

	static void run(typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {
		RecvBye *t = reinterpret_cast<RecvBye *>(state.stateData);
		t->fun(state, pkt, funIface);

		if (state.state == States::Terminate) {
			delete (t);
		}
	}
};
}; // namespace Client
}; // namespace HelloBye2

/*
 * YCM definition is a workaround for a libclang bug
 * When compiling, YCM should never be set.
 * Set YCM in a libclang based IDE in order to avoid errors
 */
#ifndef YCM
#include "../src/helloBye2Proto.cpp"
#endif

#endif /* HELLOBYEPROTO_HPP */
