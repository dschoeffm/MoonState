#ifndef MEASURE_HPP
#define MEASURE_HPP

#include <cstdint>

union tsc_t {
	uint64_t tsc_64;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	};
};

static union {
	uint64_t tsc_64;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	};
} tsc;

inline uint64_t start_measurement(void) {

	asm volatile("CPUID\n\t"
				 "RDTSC\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 : "=r"(tsc.hi_32), "=r"(tsc.lo_32)::"%rax", "%rbx", "%rcx", "%rdx");

	return tsc.tsc_64;
}

inline uint64_t stop_measurement(void) {

	asm volatile("RDTSCP\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 "CPUID\n\t"
				 : "=r"(tsc.hi_32), "=r"(tsc.lo_32)::"%rax", "%rbx", "%rcx", "%rdx");

	return tsc.tsc_64;
}

inline static uint64_t read_rdtsc() {
	union tsc_t tsc;
	asm volatile("RDTSC\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 : "=r"(tsc.hi_32), "=r"(tsc.lo_32)::"%rax", "%rbx", "%rcx", "%rdx");
	return tsc.tsc_64;
}

struct measureData {
	uint64_t openssl;
	uint64_t denseMap;
	uint64_t tbb;
	uint64_t siphash;
	uint64_t memory;
	uint64_t numPkts;
	uint64_t numBytes;
	uint64_t tx;
	uint64_t rx;
};

extern struct measureData measureData;

#endif /* MEASURE_HPP */
