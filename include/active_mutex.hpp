/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Zeeno from Atlantis wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Zeeno from Atlantis
 * ----------------------------------------------------------------------------
 */

#ifndef _ZEENO_ACTIVE_MUTEX_HPP
#define _ZEENO_ACTIVE_MUTEX_HPP
#pragma once

#include <atomic>

#if defined(__has_feature) && __has_feature(thread_sanitizer)
/*
 * When using thread sanitizer, we care about corectness of the software,
 * not a performance. Thread sanitizer does not detect operations on the atomic
 * variables as synchronization, so we need to use proper mutex.
*/
#	include <mutex>
using active_mutex = std::mutex;
#else
class active_mutex {
	std::atomic_bool flag{false};

public:
	void lock() noexcept {
		for (;;) {
			// Optimistically assume the lock is free on the first try
			if (!flag.exchange(true, std::memory_order_acquire)) {
				return;
			}
			// Wait for lock to be released without generating cache misses
			while (flag.load(std::memory_order_relaxed)) {
// Issue X86 PAUSE (TODO: or ARM YIELD) instruction to reduce contention between
// hyper-threads
#	ifdef _MSC_VER
				_mm_pause();
#	else
				__builtin_ia32_pause();
#	endif
			}
		}
	}

	bool try_lock() noexcept {
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !flag.load(std::memory_order_relaxed) && !flag.exchange(true, std::memory_order_acquire);
	}

	void unlock() noexcept {
		flag.store(false, std::memory_order_release);
	}
};
#endif
#endif // _ZEENO_ACTIVE_MUTEX_HPP
