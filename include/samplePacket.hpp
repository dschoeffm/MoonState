#ifndef SAMPLE_PACKET_HPP
#define SAMPLE_PACKET_HPP

#include <cstdint>
#include <cstdlib>

/*! Example for a packet class
 * This class is meant to show, which functions are expected to be exposed by
 * any packet class
 */
class SamplePacket {
private:
	void *data;
	uint16_t dataLen;

public:
	SamplePacket(const SamplePacket &p) : data(p.data), dataLen(p.dataLen){};
	SamplePacket() : data(nullptr), dataLen(0){};
	SamplePacket(void *data, uint16_t dataLen) : data(data), dataLen(dataLen){};
	~SamplePacket() { free(data); }

	void *getData() { return data; };
	uint16_t getDataLen() { return dataLen; };
	void setDataLen(uint16_t l) { dataLen = l; };
};

#endif /* SAMPLE_PACKET_HPP */
