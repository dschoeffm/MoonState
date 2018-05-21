#include <iostream>

#include "IPv4_5TupleL2Ident.hpp"
#include "measure.hpp"
#include "samplePacket.hpp"

template <class Ident> uint64_t runTest(int numRuns) {
	if (numRuns < 1) {
		std::cout << "numRuns needs to be bigger than 1" << std::endl;
		std::abort();
	}
	typename Ident::ConnectionID c;
	typename Ident::Hasher hasher;

	// uint64_t start = start_measurement();
	uint64_t start = read_rdtsc();
	for (int i = 0; i < numRuns; i++) {
		hasher(c);
	}
	// uint64_t stop = stop_measurement();
	uint64_t stop = read_rdtsc();

	return stop - start;
};

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " <numRuns>" << std::endl;
		std::exit(0);
	}

	int numRuns = atoi(argv[1]);

	std::cout << numRuns << ","
			  << runTest<IPv4_5TupleL2Ident<SamplePacket>>(numRuns) / numRuns << std::endl;
	return 0;
}
