#ifndef HEADERS_HPP
#define HEADERS_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sstream>
#include <string>

namespace Headers {

struct Ethernet : public ether_header {
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct ether_addr));
	}

	static std::string addrToStr(uint8_t *addr) {
		std::stringstream str;
		str << std::hex << static_cast<int>(*(addr + 0)) << ":" << std::hex
			<< static_cast<int>(*(addr + 1)) << ":" << std::hex
			<< static_cast<int>(*(addr + 2)) << ":" << std::hex
			<< static_cast<int>(*(addr + 3)) << ":" << std::hex
			<< static_cast<int>(*(addr + 4)) << ":" << std::hex
			<< static_cast<int>(*(addr + 5));
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->ether_shost); }

	std::string getDstAddr() { return addrToStr(this->ether_dhost); }

	uint16_t getEthertype() { return ntohs(this->ether_type); }

	void setSrcAddr(std::array<uint8_t, 6> addr) {
		memcpy(this->ether_shost, addr.data(), 6);
	}

	void setDstAddr(std::array<uint8_t, 6> addr) {
		memcpy(this->ether_dhost, addr.data(), 6);
	}

	void setEthertype(uint16_t type) { this->ether_type = htons(type); }
};

struct IPv4 : public ip {
	void *getPayload() {
		return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(this) + 4 * this->ip_hl);
	}

	static std::string addrToStr(struct in_addr addr) {
		std::stringstream str;
		str << (addr.s_addr >> 24) << "." << ((addr.s_addr >> 16) & 0xff) << "."
			<< ((addr.s_addr >> 8) & 0xff) << "." << (addr.s_addr & 0xff);
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->ip_src); }

	std::string getDstAddr() { return addrToStr(this->ip_dst); }

	void setLength(uint16_t len) { this->ip_len = htons(len); }

	uint16_t getLength() { return ntohs(this->ip_len); }

	void calcChecksum() {
		uint32_t result = 0;
		uint16_t *hdr_cast = reinterpret_cast<uint16_t *>(this);

		this->ip_sum = 0;
		for (uint8_t i = 0; i < (this->ip_hl * 4); i++) {
			result += ntohs(hdr_cast[i]);
			if (result & (1 << 16)) {
				result &= 0xffff;
				result++;
			}
		}

		this->ip_sum = htons(~result);
	};
};

struct IPv6 : public ip6_hdr {
	void *getPayload() {
		// For now, we just assume, that there is no extension header...
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct ip6_hdr));
	}
};

struct Udp : public udphdr {
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct udphdr));
	}
};

struct Tcp : public tcphdr {
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct tcphdr));
	}
};
};

#endif /* HEADERS_HPP */
