#ifndef IPV4_5TUPLEL2IDENT_HPP
#define IPV4_5TUPLEL2IDENT_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "sodium.h"

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
			Hasher hasher;
			if (hasher(*this) < hasher(c)) {
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
		uint64_t operator()(const ConnectionID &c) const {
			uint64_t res = 0;

			/*
			res += c.dstPort;
			res += c.srcPort;
			res += c.dstIP;
			res += c.srcIP;
			res += c.proto;
			*/

			struct __attribute__((packed)) {
				uint32_t srcIP;
				uint32_t dstIP;
				uint16_t srcPort;
				uint16_t dstPort;
				uint8_t proto;
			} hashContent;

			hashContent.srcIP = c.srcIP;
			hashContent.dstIP = c.dstIP;
			hashContent.srcPort = c.srcPort;
			hashContent.dstPort = c.dstPort;
			hashContent.proto = c.proto;

			if (sizeof(hashContent) != 13) {
				throw std::runtime_error("sizeof(hashContent) != 13");
			}

			assert(crypto_shorthash_BYTES == 8);
			uint8_t key[crypto_shorthash_KEYBYTES];
			memset(key, 0, crypto_shorthash_KEYBYTES);
			crypto_shorthash(reinterpret_cast<uint8_t *>(&res),
				reinterpret_cast<const uint8_t *>(&hashContent), sizeof(hashContent), key);

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

	static ConnectionID getDelKey() {
		ConnectionID id;
		id.dstIP = 0;
		id.srcIP = 0;
		id.dstPort = 0;
		id.srcPort = 0;
		id.proto = 253;
		return id;
	};

	static ConnectionID getEmptyKey() {
		ConnectionID id;
		id.dstIP = 0;
		id.srcIP = 0;
		id.dstPort = 0;
		id.srcPort = 0;
		id.proto = 254;
		return id;
	};
};

#endif /* IPV4_5TUPLEL2IDENT_HPP */
