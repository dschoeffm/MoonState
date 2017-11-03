#include <cstdint>

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

using StateID = uint16_t;

void hexdump(const void* data, unsigned int dataLen);
