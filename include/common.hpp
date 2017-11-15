#include <cstdint>

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

#ifdef DEBUG
#define PROD_INLINE
#else
#define PROD_INLINE inline __attribute__((always_inline))
#endif

using StateID = uint16_t;

void hexdump(const void *data, int dataLen);
