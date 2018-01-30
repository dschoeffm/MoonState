#ifndef COMMON_HPP
#define COMMON_HPP

#include <array>
#include <cassert>

#ifdef DEBUG
#define DEBUG_ENABLED(x) x
#else
#define DEBUG_ENABLED(x)
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

/*! This is a 48 bit variable
 *
 * It is intendet to be used like uint*_t.
 * The assignment are cast operators expect uint64_t.
 */
struct uint48 {

	std::array<uint8_t, 6> arr;

	void set(uint64_t val) {
		assert((val >> 48) == 0);
		arr[0] = static_cast<uint8_t>(val & 0xff);
		arr[1] = static_cast<uint8_t>((val >> 8) & 0xff);
		arr[2] = static_cast<uint8_t>((val >> 16) & 0xff);
		arr[3] = static_cast<uint8_t>((val >> 24) & 0xff);
		arr[4] = static_cast<uint8_t>((val >> 32) & 0xff);
		arr[5] = static_cast<uint8_t>((val >> 40) & 0xff);
	};

	uint64_t get() {
		uint64_t val = 0;

		val |= static_cast<uint64_t>(arr[0]);
		val = val << 8;

		val |= static_cast<uint64_t>(arr[1]);
		val = val << 8;

		val |= static_cast<uint64_t>(arr[2]);
		val = val << 8;

		val |= static_cast<uint64_t>(arr[3]);
		val = val << 8;

		val |= static_cast<uint64_t>(arr[4]);
		val = val << 8;

		val |= static_cast<uint64_t>(arr[5]);

		return val;
	};

	operator uint64_t() { return get(); };

	uint64_t operator=(uint64_t val) {
		set(val);
		return val;
	};
};

/*! This is a 24 bit variable
 *
 * It is intendet to be used like uint*_t.
 * The assignment are cast operators expect uint32_t.
 */
struct uint24 {

	std::array<uint8_t, 3> arr;

	void set(uint32_t val) {
		assert((val >> 24) == 0);
		arr[0] = static_cast<uint8_t>(val & 0xff);
		arr[1] = static_cast<uint8_t>((val >> 8) & 0xff);
		arr[2] = static_cast<uint8_t>((val >> 16) & 0xff);
	};

	uint32_t get() {
		uint32_t val = 0;

		val |= static_cast<uint32_t>(arr[0]);
		val = val << 8;

		val |= static_cast<uint32_t>(arr[1]);
		val = val << 8;

		val |= static_cast<uint32_t>(arr[2]);

		return val;
	};

	operator uint32_t() { return get(); };

	uint32_t operator=(uint32_t val) {
		set(val);
		return val;
	};
};

#endif /* COMMON_HPP */
