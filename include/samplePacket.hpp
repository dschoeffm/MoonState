#ifndef SAMPLE_PACKET_HPP
#define SAMPLE_PACKET_HPP

#include <cstdint>

class SamplePacket {
private:
	void *data;
	uint32_t dataLen;

public:
	SamplePacket(const SamplePacket &p) : data(p.data), dataLen(p.dataLen){};
	SamplePacket() : data(nullptr), dataLen(0){};
	SamplePacket(void* data, uint32_t dataLen) : data(data), dataLen(dataLen){};

	void *getData() { return data; };
	uint32_t getDataLen() { return dataLen; };
};

#endif /* SAMPLE_PACKET_HPP */
