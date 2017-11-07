#ifndef HEADERS_HPP
#define HEADERS_HPP

#include <cstdint>
#include <array>
#include <string>
#include <sstream>
#include <cstring>

#include <arpa/inet.h>

namespace Headers {

/*! Representation of the etherner header.
 */
struct Ethernet {
	std::array<uint8_t, 6> destMac; //!< Destination MAC
	std::array<uint8_t, 6> srcMac; //!< Source MAC
	uint16_t ethertype; //!< Ethertype

	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Ethernet));
	}

	static std::string addrToStr(std::array<uint8_t, 6> addr) {
		std::stringstream str;
		str << std::hex << static_cast<int>(addr[0]) << ":" << std::hex
			<< static_cast<int>(addr[1]) << ":" << std::hex
			<< static_cast<int>(addr[2]) << ":" << std::hex
			<< static_cast<int>(addr[3]) << ":" << std::hex
			<< static_cast<int>(addr[4]) << ":" << std::hex
			<< static_cast<int>(addr[5]);
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->srcMac); }

	std::string getDstAddr() { return addrToStr(this->destMac); }

	uint16_t getEthertype() { return ntohs(this->ethertype); }

	void setSrcAddr(const std::array<uint8_t, 6> addr) {
		memcpy(this->srcMac.data(), addr.data(), 6);
	}

	void setDstAddr(std::array<uint8_t, 6> addr) {
		memcpy(this->destMac.data(), addr.data(), 6);
	}

	void setEthertype(uint16_t type) { this->ethertype = htons(type); }
} __attribute__((packed));

/*! Representation of the IPv4 header.
 */
struct IPv4 {
	uint8_t version_ihl; //!< Version and IHL

	uint8_t version() const {
		return (version_ihl & 0xf0) >> 4;
	}
	uint8_t ihl() const {
		return version_ihl & 0x0f;
	}

	uint8_t tos; //!< Type of Service
	uint16_t total_length; //!< L3-PDU length
	uint16_t id; //!< Identification
	uint16_t flags_fragmentation; //!< flags and fragmentation offset

	uint16_t fragmentation() const {
		return flags_fragmentation & 0x1fff;
	}
	uint16_t flags() const {
		return flags_fragmentation >> 18;
	}

	uint8_t ttl; //!< Time to live
	uint8_t proto; //!< next protocol

	static constexpr uint8_t PROTO_ICMP = 1;
	static constexpr uint8_t PROTO_TCP = 6;
	static constexpr uint8_t PROTO_UDP = 17;

	uint16_t checksum; //!< header checksum
	uint32_t srcIP; //!< source IPv4 address
	uint32_t dstIP; //!< destination IPv4 address

	void *getPayload() {
		return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(this) + 4*ihl());
	}

	static std::string addrToStr(uint32_t addr) {
		std::stringstream str;
		str << (addr >> 24) << "." << ((addr >> 16) & 0xff) << "."
			<< ((addr >> 8) & 0xff) << "." << (addr & 0xff);
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->srcIP); }

	std::string getDstAddr() { return addrToStr(this->dstIP); }

	void setLength(uint16_t len) { this->total_length = htons(len); }

	uint16_t getLength() { return ntohs(this->total_length); }

	void calcChecksum() {
		uint32_t result = 0;
		uint16_t *hdr_cast = reinterpret_cast<uint16_t *>(this);

		this->checksum = 0;
		for (uint8_t i = 0; i < (this->ihl() * 4); i++) {
			result += ntohs(hdr_cast[i]);
			if (result & (1 << 16)) {
				result &= 0xffff;
				result++;
			}
		}

		this->checksum = htons(~result);
	};

} __attribute__((packed));


/*! Representation of the TCP header.
 */
struct Tcp {
	uint16_t srcPort; //!< Source port
	uint16_t dstPort; //!< Destination port
	uint32_t seq; //!< Sequence number
	uint32_t ack; //!< Acknowledgement number
	uint16_t offset_flags; //!< Data offset and flags
	uint16_t window; //!< Receive window
	uint16_t checksum; //!< Checksum
	uint16_t urgend_ptr; //!< Urgent pointer

	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Tcp));
	}

} __attribute__((packed));

/*! Representation of the UDP header.
 */
struct Udp {
	uint16_t srcPort; //!< Source port
	uint16_t dstPort; //!< Destination port
	uint16_t len; //!< Length
	uint16_t checksum; //!< Checksum

	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Udp));
	}

} __attribute__((packed));

/*! Representation of the ICMP header
 */
struct Icmp {
	uint8_t type; //!< Type
	uint8_t code; //!< Code
	uint16_t checksum; //!< Checksum
} __attribute__((packed));

}


#endif /* HEADERS_HPP */
