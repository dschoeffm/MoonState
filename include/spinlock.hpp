#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP

#include <atomic>

class SpinLock {
private:
	std::atomic_flag latch = ATOMIC_FLAG_INIT;

public:
	SpinLock(){};

	bool trylock() { return !latch.test_and_set(std::memory_order::memory_order_acquire); }

	void lock() {
		while (!trylock()) {
		}
	};

	void unlock() { latch.clear(std::memory_order::memory_order_release); }
};

class SpinLockCLSize {
private:
	std::atomic_flag latch = ATOMIC_FLAG_INIT;
	volatile char c[64 - sizeof(std::atomic_flag)];

public:
	SpinLockCLSize(){};

	bool trylock() { return !latch.test_and_set(std::memory_order::memory_order_acquire); }

	void lock() {
		while (!trylock()) {
		}
	};

	void unlock() { latch.clear(std::memory_order::memory_order_release); }
};

#endif /* SPINLOCK_HPP */
