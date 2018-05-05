#ifndef HEADERS_HPP
#define HEADERS_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#include <arpa/inet.h>

namespace Headers {

/*! Representation of the etherner header. */
struct Ethernet {
	std::array<uint8_t, 6> destMac; //!< Destination MAC
	std::array<uint8_t, 6> srcMac;  //!< Source MAC
	uint16_t ethertype;				//!< Ethertype

	/*! Get the SDU
	 * \return Pointer to the payload
	 */
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Ethernet));
	}

	static std::string addrToStr(std::array<uint8_t, 6> addr) {
		std::stringstream str;
		str << std::hex << static_cast<int>(addr[0]) << ":" << std::hex
			<< static_cast<int>(addr[1]) << ":" << std::hex << static_cast<int>(addr[2])
			<< ":" << std::hex << static_cast<int>(addr[3]) << ":" << std::hex
			<< static_cast<int>(addr[4]) << ":" << std::hex << static_cast<int>(addr[5]);
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->srcMac); }

	std::string getDstAddr() { return addrToStr(this->destMac); }

	/*! Get the ethertype
	 * \return Ethertype in host byte order
	 */
	uint16_t getEthertype() { return ntohs(this->ethertype); }

	static constexpr uint16_t ETHERTYPE_IPv4 = 0x0800;
	static constexpr uint16_t ETHERTYPE_IPv6 = 0x86dd;

	/*! Set the ethertype
	 * \param type Ethertype in host byte order
	 */
	void setEthertype(uint16_t type) { this->ethertype = htons(type); }

	void setSrcAddr(const std::array<uint8_t, 6> addr) {
		memcpy(this->srcMac.data(), addr.data(), 6);
	}

	void setDstAddr(std::array<uint8_t, 6> addr) {
		memcpy(this->destMac.data(), addr.data(), 6);
	}

} __attribute__((packed));

/*! Representation of the IPv4 header. */
struct IPv4 {
	uint8_t version_ihl; //!< Version and IHL

	uint8_t version() const {
		uint8_t ret = (version_ihl & 0xf0) >> 4;
		assert(ret >= 4);
		return ret;
	}
	uint8_t ihl() const { return version_ihl & 0x0f; }

	/*! Set the version field to 4
	 */
	void setVersion() {
		version_ihl &= 0x0f;
		version_ihl |= 0x40;
	}

	/*! Sets the IP header length
	 * \param len length as it will appear in the header
	 */
	void setIHL(uint8_t len) {
		version_ihl &= 0xf0;
		len &= 0x0f;
		version_ihl |= len;
	}

	uint8_t tos;		   //!< Type of Service
	uint16_t total_length; //!< L3-PDU length

	/*! Set the length of the L3-SDU
	 * \param len of the L3-SDU (IP payload length)
	 */
	void setPayloadLength(uint16_t len) { total_length = htons(ihl() * 4 + len); }

	/*! Get the length of the L3-SDU
	 * \return The length of the payload (host byte order)
	 */
	uint16_t getPayloadLength() const { return ntohs(total_length) - ihl() * 4; }

	uint16_t id;				  //!< Identification
	uint16_t flags_fragmentation; //!< flags and fragmentation offset

	uint16_t fragmentation() const { return flags_fragmentation & 0x1fff; }
	uint16_t flags() const { return flags_fragmentation >> 18; }

	uint8_t ttl;   //!< Time to live
	uint8_t proto; //!< next protocol

	static constexpr uint8_t PROTO_ICMP = 1;
	static constexpr uint8_t PROTO_TCP = 6;
	static constexpr uint8_t PROTO_UDP = 17;

	/*! Set the protocol to ICMP */
	void setProtoICMP() { proto = PROTO_ICMP; }

	/*! Set the protocol to TCP */
	void setProtoTCP() { proto = PROTO_TCP; }

	/*! Set the protocol to UDP */
	void setProtoUDP() { proto = PROTO_UDP; }

	uint16_t checksum; //!< header checksum
	uint32_t srcIP;	//!< source IPv4 address
	uint32_t dstIP;	//!< destination IPv4 address

	/*! Set the source ip
	 * \param ip Source IP in host byte order
	 */
	void setSrcIP(uint32_t ip) { srcIP = htonl(ip); }

	/*! Set the destination ip
	 * \param ip Destination IP in host byte order
	 */
	void setDstIP(uint32_t ip) { dstIP = htonl(ip); }

	/*! Get the source IP
	 * \return Source IP in host byte order
	 */
	uint32_t getSrcIP() const { return ntohl(srcIP); }

	/*! Get the destination IP
	 * \return Destination IP in host byte order
	 */
	uint32_t getDstIP() const { return ntohl(dstIP); }

	/*! Get the SDU
	 * \return Pointer to the payload
	 */
	void *getPayload() {
		return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(this) + 4 * ihl());
	}

	static std::string addrToStr(uint32_t addr) {
		std::stringstream str;
		str << (addr >> 24) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 8) & 0xff)
			<< "." << (addr & 0xff);
		return str.str();
	}

	std::string getSrcAddr() { return addrToStr(this->srcIP); }

	std::string getDstAddr() { return addrToStr(this->dstIP); }

	void setLength(uint16_t len) { this->total_length = htons(len); }

	uint16_t getLength() { return ntohs(this->total_length); }

	/*! Fill out the header checksum for this packet */
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

/*! Representation of the TCP header. */
struct Tcp {
	uint16_t srcPort;		 //!< Source port
	uint16_t dstPort;		 //!< Destination port
	uint32_t seq;			 //!< Sequence number
	uint32_t ack;			 //!< Acknowledgement number
	uint8_t offset_reserved; //!< Data offset and reserved
	uint8_t flags;			 //!< Flags
	uint16_t window;		 //!< Receive window
	uint16_t checksum;		 //!< Checksum
	uint16_t urgend_ptr;	 //!< Urgent pointer

	/*! Get the SDU
	 * \return Pointer to the payload
	 */
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Tcp));
	}

	/*! Set the source port
	 * \param p Source port in host byte order
	 */
	void setSrcPort(uint16_t p) { srcPort = htons(p); }

	/*! Set the destination port
	 * \param p Destination port in host byte order
	 */
	void setDstPort(uint16_t p) { dstPort = htons(p); }

	/*! Get the source port
	 * \return Source port in host byte order
	 */
	uint16_t getSrcPort() const { return ntohs(srcPort); }

	/*! Get the destination port
	 * \return Destination port in host byte order
	 */
	uint16_t getDstPort() const { return ntohs(dstPort); }

	uint32_t getSeq() const { return ntohl(seq); }
	uint32_t getAck() const { return ntohl(ack); }

	void setSeq(uint32_t s) { seq = htonl(s); }
	void setAck(uint32_t a) { ack = htonl(a); }

	uint16_t getWindow() { return ntohs(window); }
	void setWindow(uint16_t w) { window = htons(w); }

	void clearFlags() { flags = 0; }

	bool getFinFlag() const { return flags & 0x1; }
	bool getSynFlag() const { return flags & 0x2; }
	bool getRstFlag() const { return flags & 0x4; }
	bool getAckFlag() const { return flags & 0x10; }

	void setFinFlag() { flags |= 0x1; }
	void setSynFlag() { flags |= 0x2; }
	void setRstFlag() { flags |= 0x4; }
	void setAckFlag() { flags |= 0x10; }

	void setOffset(uint8_t offset) {
		offset = offset << 4;
		offset_reserved = offset;
	}

	uint8_t getHeaderLen() {
		uint8_t tmp = offset_reserved;
		// This *should* get compiled into one shift (>>2)
		tmp = tmp >> 4;
		tmp = tmp * 4;
		return tmp;
	}
} __attribute__((packed));

/*! Representation of the UDP header. */
struct Udp {
	uint16_t srcPort;  //!< Source port
	uint16_t dstPort;  //!< Destination port
	uint16_t len;	  //!< Length
	uint16_t checksum; //!< Checksum

	/*! Set the source port
	 * \param p Source port in host byte order
	 */
	void setSrcPort(uint16_t p) { srcPort = htons(p); }

	/*! Set the destination port
	 * \param p Destination port in host byte order
	 */
	void setDstPort(uint16_t p) { dstPort = htons(p); }

	/*! Get the source port
	 * \return Source port in host byte order
	 */
	uint16_t getSrcPort() const { return ntohs(srcPort); }

	/*! Get the destination port
	 * \return Destination port in host byte order
	 */
	uint16_t getDstPort() const { return ntohs(dstPort); }

	/*! Get the length of the L4-SDU (UDP payload)
	 * \return Length of the payload (host byte order)
	 */
	uint16_t getPayloadLength() const { return ntohs(len) - sizeof(struct Udp); }

	/*! Set the length of the L4-SDU (UDP payload)
	 * \param length of the payload (host byte order)
	 */
	void setPayloadLength(uint16_t length) { len = htons(length + sizeof(struct Udp)); }

	/*! Get the SDU
	 * \return Pointer to the payload
	 */
	void *getPayload() {
		return reinterpret_cast<void *>(
			reinterpret_cast<uint8_t *>(this) + sizeof(struct Udp));
	}

} __attribute__((packed));

/*! Representation of the ICMP header */
struct Icmp {
	uint8_t type; //!< Type

	static constexpr uint8_t TYPE_ECHO_REPLY = 0;
	static constexpr uint8_t TYPE_ECHO = 8;

	uint8_t code;	  //!< Code
	uint16_t checksum; //!< Checksum

	union {
		struct {
			uint16_t identifier;
			uint16_t seqNumber;
		} echo;
	} payload;

} __attribute__((packed));

} // namespace Headers

#endif /* HEADERS_HPP */
