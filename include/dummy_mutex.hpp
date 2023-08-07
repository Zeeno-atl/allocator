#pragma once

struct dummy_mutex {
	void lock() {
	}
	void unlock() {
	}
	bool try_lock() {
		return true;
	}
};
