#ifndef IPV4_5TUPLEL2IDENT_HPP
#define IPV4_5TUPLEL2IDENT_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>

// IPv4/UDP/TCP header
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "exceptions.hpp"

template <class Packet> class IPv4_5TupleL2Ident {
public:
	struct Hasher;
	struct ConnectionID {
		uint32_t dstIP;
		uint32_t srcIP;
		uint16_t dstPort;
		uint16_t srcPort;
		uint8_t proto;

		bool operator==(const ConnectionID &c) const {
			if ((dstIP == c.dstIP) && (srcIP == c.srcIP) && (dstPort == c.dstPort) &&
				(srcPort == c.srcPort) && (proto == c.proto)) {
				return true;
			} else {
				return false;
			}
		};

		bool operator<(const ConnectionID &c) const {
			if (Hasher(this) < Hasher(c)) {
				return true;
			} else {
				return false;
			}
		};

		ConnectionID(const ConnectionID &c)
			: dstIP(c.dstIP), srcIP(c.srcIP), dstPort(c.dstPort), srcPort(c.srcPort),
			  proto(c.proto){};
		ConnectionID() : dstIP(0), srcIP(0), dstPort(0), srcPort(0), proto(0){};
	};

	struct Hasher {
		// In the future, maybe we find something more random...
		size_t operator()(const ConnectionID &c) const {
			uint32_t res = c.dstPort;
			res |= c.srcPort << 16;
			res ^= c.dstIP;
			res += c.srcIP;
			res += c.proto;
			return res;
		}
	};

	static ConnectionID identify(Packet *pkt) {
		ConnectionID id;
		struct ip *ip = reinterpret_cast<struct ip *>(reinterpret_cast<uint8_t*>(pkt->getData()) + 14);
		id.dstIP = ip->ip_dst.s_addr;
		id.srcIP = ip->ip_src.s_addr;
		id.proto = ip->ip_p;

		if (id.proto == IPPROTO_UDP) {
			struct udphdr *udp = reinterpret_cast<struct udphdr *>(ip + ip->ip_hl * 4);
			id.dstPort = udp->dest;
			id.srcPort = udp->source;
		} else if (id.proto == IPPROTO_TCP) {
			struct tcphdr *tcp = reinterpret_cast<struct tcphdr *>(ip + ip->ip_hl * 4);
			id.dstPort = tcp->dest;
			id.srcPort = tcp->source;
		} else {
			throw new PacketNotIdentified();
		}

		return id;
	};
};

#endif /* IPV4_5TUPLEL2IDENT_HPP */
