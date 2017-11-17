#ifndef IPV4_5TUPLEL2IDENT_HPP
#define IPV4_5TUPLEL2IDENT_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "headers.hpp"

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

		operator std::string() const {
			std::stringstream sstream;
			sstream << "DstIP: " << std::hex << ntohl(dstIP) << ", ";
			sstream << "SrcIP: " << std::hex << ntohl(srcIP) << ", ";
			sstream << "DstPort: " << std::dec << ntohs(dstPort) << ", ";
			sstream << "SrcPort: " << std::dec << ntohs(srcPort);
			return sstream.str();
		}

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
		struct Headers::IPv4 *ip = reinterpret_cast<struct Headers::IPv4 *>(
			reinterpret_cast<uint8_t *>(pkt->getData()) + 14);
		id.dstIP = ip->dstIP;
		id.srcIP = ip->srcIP;
		id.proto = ip->proto;

		if (id.proto == IPPROTO_UDP) {
			struct Headers::Udp *udp =
				reinterpret_cast<struct Headers::Udp *>(ip->getPayload());
			id.dstPort = udp->dstPort;
			id.srcPort = udp->srcPort;
		} else if (id.proto == IPPROTO_TCP) {
			struct Headers::Tcp *tcp =
				reinterpret_cast<struct Headers::Tcp *>(ip->getPayload());
			id.dstPort = tcp->dstPort;
			id.srcPort = tcp->srcPort;
		} else {
#ifdef DEBUG
			std::cout << "IPv4_5TupleL2Ident::indentify() failed" << std::endl;
			std::cout << "IPv4_5TupleL2Ident::indentify() id.proto="
					  << static_cast<unsigned int>(id.proto) << std::endl;
#endif
			hexdump(pkt->getData(), pkt->getDataLen());
			throw new PacketNotIdentified();
		}

		return id;
	};
};

#endif /* IPV4_5TUPLEL2IDENT_HPP */
