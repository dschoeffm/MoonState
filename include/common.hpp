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

/*! Dump hex data
 *
 * \param data Pointer to the data to dump
 * \param dataLen Length of the data to dump
 */
void hexdump(const void *data, int dataLen);
