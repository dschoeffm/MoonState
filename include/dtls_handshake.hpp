#ifndef DTLS_HANDSHAKE_HPP
#define DTLS_HANDSHAKE_HPP

#include "dtls.hpp"

namespace DTLS {

namespace Handshake {
struct HandshakeType {
	static constexpr uint8_t helloRequest = 0;
	static constexpr uint8_t clientHello = 1;
	static constexpr uint8_t serverHello = 2;
	static constexpr uint8_t certificate = 11;
	static constexpr uint8_t serverKeyExchange = 12;
	static constexpr uint8_t certificateRequest = 13;
	static constexpr uint8_t serverHelloDone = 14;
	static constexpr uint8_t certificateVerify = 15;
	static constexpr uint8_t clientKeyExchange = 16;
	static constexpr uint8_t finished = 20;

	uint8_t val;

	void setHelloRequest() { val = helloRequest; };
	void setClientHello() { val = clientHello; };
	void setServerHello() { val = serverHello; };
	void setCertificate() { val = certificate; };
	void setServerKeyExchange() { val = serverKeyExchange; };
	void setCertificateRequest() { val = certificateRequest; };
	void setServerHelloDone() { val = serverHelloDone; };
	void setCertificateVerify() { val = certificateVerify; };
	void setClientKeyExchange() { val = clientKeyExchange; };
	void setFinished() { val = finished; };

	bool isHelloRequest() { return val == helloRequest; };
	bool isClientHello() { return val == clientHello; };
	bool isServerHello() { return val == serverHello; };
	bool isCertificate() { return val == certificate; };
	bool isServerKeyExchange() { return val == serverKeyExchange; };
	bool isCertificateRequest() { return val == certificateRequest; };
	bool isServerHelloDone() { return val == serverHelloDone; };
	bool isCertificateVerify() { return val == certificateVerify; };
	bool isClientKeyExchange() { return val == clientKeyExchange; };
	bool isFinished() { return val == finished; };
};

struct Header {
	HandshakeType msgType;
	uint24 length;
	uint16_t messageSeq;
	uint24 fragmentOffset;
	uint24 fragmentLength;
};

namespace Hello {};
namespace Certificate {};
namespace KeyExchange {};
namespace Finished {};

}; // namespace Handshake

}; // namespace DTLS

#endif /* DTLS_HANDSHAKE_HPP */
