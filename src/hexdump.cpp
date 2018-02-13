#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "common.hpp"

using namespace std;

void hexdump(const void *data, int dataLen) {
	const char *const start = static_cast<const char *>(data);

	int it = 0;

	stringstream all;

	cout << "Hexdump of: " << data << endl;
	while (dataLen > 0) {
		unsigned int lineLength = min((int)16, dataLen);
		const char *line = start + it;

		stringstream ascii;
		stringstream hexDump;

		ascii << "0x" << setfill('0') << setw(4) << hex << it << " : ";

		for (unsigned int i = 0; i < lineLength; i++) {
			char c = static_cast<char>(line[i]);
			unsigned int varInt = 0;
			varInt |= static_cast<uint8_t>(c);

			if (isprint(varInt) != 0) {
				ascii << static_cast<char>(line[i]);
			} else {
				ascii << '.';
			}

			hexDump << " " << setfill('0') << setw(2) << hex << uppercase << varInt;
		}

		for (int i = lineLength; i < 16; i++) {
			ascii << " ";
		}

		all << ascii.str();
		all << hexDump.str();
		all << endl;

		dataLen -= 16;
		it += lineLength;
	}

	cout << all.str();
}

void hexdumpHexOnly(const void *data, int dataLen) {
	const char *const start = static_cast<const char *>(data);

	int it = 0;

	stringstream all;

	cout << "Hexdump of: " << data << endl;
	while (dataLen > 0) {
		unsigned int lineLength = min((int)16, dataLen);
		const char *line = start + it;

		for (unsigned int i = 0; i < lineLength; i++) {
			char c = static_cast<char>(line[i]);
			unsigned int varInt = 0;
			varInt |= static_cast<uint8_t>(c);

			all << " " << setfill('0') << setw(2) << hex << uppercase << varInt;
		}

		all << endl;

		dataLen -= 16;
		it += lineLength;
	}

	cout << all.str();
}
