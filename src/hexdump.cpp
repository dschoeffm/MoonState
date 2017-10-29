
/*
 * This hexdump is based on an implementation aquired from:
 * http://www.i42.co.uk/stuff/hexdump.htm
 *
 */

#include <iostream>
#include <string>

#include "common.hpp"

using namespace std;

void hexdump(const void *data, unsigned int dataLen) {
	const char *const start = static_cast<const char *>(data);
	const char *const end = start + dataLen;
	const char *line = start;

	while (line != end) {
		cout.width(4);
		cout.fill('0');
		cout << std::hex << line - start << " : ";
		unsigned int lineLength = min((unsigned int) 16, static_cast<unsigned int>(end - line));

		for (unsigned int pass = 1; pass <= 2; ++pass) {
			for (const char *next = line; next != end && next != line + 16; ++next) {
				char ch = *next;
				switch (pass) {
				case 1:
					cout << (ch < 32 ? '.' : ch);
					break;
				case 2:
					if (next != line)
						cout << " ";
					cout.width(2);
					cout.fill('0');
					cout << std::hex << std::uppercase
						 << static_cast<int>(static_cast<unsigned char>(ch));
					break;
				}
			}
			if (pass == 1 && lineLength != 16) {
				cout << string(16 - lineLength, ' ');
			}
			cout << " ";
		}
		cout << endl;
		line = line + lineLength;
	}
}
