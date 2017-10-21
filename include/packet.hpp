#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>

struct Packet {
	void* data;
	uint32_t dataLen;
};

#endif /* PACKET_HPP */
