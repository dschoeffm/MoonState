#ifndef DTLS_RECORD_HPP
#define DTLS_RECORD_HPP

#include "dtls.hpp"

namespace DTLS {

namespace Record {

struct ProtocolVersion {
	static constexpr uint8_t major12 = 254;
	static constexpr uint8_t minor12 = 253;

	uint8_t major;
	uint8_t minor;

	void set() {
		major = major12;
		minor = minor12;
	};

	void set(uint8_t majorSet, uint8_t minorSet) {
		major = majorSet;
		minor = minorSet;
	};

	bool check() {
		if ((major == major12) && (minor == minor12)) {
			return true;
		} else {
			return false;
		}
	}
};

struct ContentType {
	static constexpr uint8_t changeCipherSpec = 20;
	static constexpr uint8_t alert = 21;
	static constexpr uint8_t handshake = 22;
	static constexpr uint8_t applicationData = 23;

	uint8_t val;

	void setChangeCipherSpec() { val = changeCipherSpec; };
	void setAlert() { val = alert; };
	void setHandshake() { val = handshake; };
	void setApplicationData() { val = applicationData; };

	bool isChangeCipherSpec() { return val == changeCipherSpec; };
	bool isAlert() { return val == alert; };
	bool isHandshake() { return val == handshake; };
	bool isApplicationData() { return val == applicationData; };
};

struct Header {
	ContentType type;
	ProtocolVersion version;
	uint16_t epoch;
	uint48 sequenceNumber;
	uint16_t length;

	void setEpoch(uint16_t val) { epoch = val; };
	void setSequenceNumber(uint48 val) { sequenceNumber = val; };
	void setLength(uint16_t val) { length = val; };

	uint16_t getEpoch() { return epoch; };
	uint48 getSequenceNumber() { return sequenceNumber; };
	uint16_t getLength() { return length; };
};

}; // namespace Record
}; // namespace DTLS

#endif /* DTLS_RECORD_HPP */
