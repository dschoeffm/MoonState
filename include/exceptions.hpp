#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <exception>
#include <stdexcept>

class PacketNotIdentified : public std::runtime_error {
public:
	PacketNotIdentified()
		: runtime_error("The packet identifier could not handle this packet"){};
};

#endif /* EXCEPTIONS_HPP */
