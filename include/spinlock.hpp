#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP

#include <atomic>

class __attribute__((aligned(64))) SpinLock {
private:
	std::atomic_flag latch = ATOMIC_FLAG_INIT;

public:
	SpinLock(){};

	bool trylock() { return !latch.test_and_set(std::memory_order::memory_order_acquire); }

	void lock() {
		while (!trylock()) {
			__asm__ ( "pause;" );
		}
	};

	void unlock() { latch.clear(std::memory_order::memory_order_release); }
};

class SpinLockCLSize {
private:
	// The useful part
	std::atomic_flag latch = ATOMIC_FLAG_INIT;

	// The padding, assumes 64 byte cacheline
	// This should prevent false sharing
	volatile char c[64 - sizeof(std::atomic_flag)];

public:
	SpinLockCLSize() {
		// This cast is just to silence clang
		(void)c;
	};

	bool trylock() { return !latch.test_and_set(std::memory_order::memory_order_acquire); }

	void lock() {
		while (!trylock()) {
			__asm__ ( "pause;" );
		}
	};

	void unlock() { latch.clear(std::memory_order::memory_order_release); }
};

#endif /* SPINLOCK_HPP */
