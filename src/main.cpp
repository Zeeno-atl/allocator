#include <chrono>
#include <deque>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <range/v3/view/iota.hpp>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

import allocator;
import pretty_name;

auto repeat(int n) {
	return ranges::iota_view(0, n);
}

constexpr auto REPEAT{1'000'000};
constexpr auto TESTS{10};

template<typename T>
void testAllocator(T& alloc) {
	std::vector<void*> v(REPEAT, nullptr);
	for ([[maybe_unused]] auto i : repeat(REPEAT)) {
		v.at(i) = std::allocator_traits<T>::allocate(alloc, 1);
	}
	for (void* m : v) {
		std::allocator_traits<T>::deallocate(alloc, static_cast<typename std::allocator_traits<T>::pointer>(m), 1);
	}
}

template<template<typename> typename Alloc, typename T>
void compilable() {
	// Check if the allocator works
	std::vector<T, Alloc<T>>                                   v;
	std::map<T, T, std::less<T>, Alloc<std::pair<const T, T>>> m;
	std::set<T, std::less<T>, Alloc<T>>                        s;
	//	std::unordered_map<T, T, std::hash<T>, std::equal_to<T>, Alloc<std::pair<const T, T>>>      um;
	//	std::unordered_set<T, std::hash<T>, std::equal_to<T>, Alloc<T>>                             us;
	std::multimap<T, T, std::less<T>, Alloc<std::pair<const T, T>>> mm;
	std::multiset<T, std::less<T>, Alloc<T>>                        ms;
	//	std::unordered_multimap<T, T, std::hash<T>, std::equal_to<T>, Alloc<std::pair<const T, T>>> umm;
	//	std::unordered_multiset<T, std::hash<T>, std::equal_to<T>, Alloc<T>>                        ums;
	//	std::deque<T, Alloc<T>>        d;
	std::list<T, Alloc<T>>         l;
	std::forward_list<T, Alloc<T>> fl;
}

template<typename Alloc>
void repeatedTest(std::function<std::shared_ptr<Alloc>()> f) {
	// Test performance
	using namespace std::chrono;
	using Clock = high_resolution_clock;

	auto start = Clock::now();
	for ([[maybe_unused]] auto i : repeat(TESTS)) {
		auto alloc = f();
		testAllocator(*alloc);
	}
	auto end = Clock::now();
	std::cout << duration_cast<microseconds>(end - start).count() << "us for " << pretty_name::pretty_name<Alloc>() << std::endl;
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char const* argv[]) -> int {
	allocator::static_size_allocator<uint32_t> a{32};
	std::clog << "Allocator element size is " << a.ELEM_SIZE << std::endl;
	std::clog << "Allocator block size is " << a._blockSize << std::endl;
	auto count_blocks = [&a] {
		auto block = a._firstBlock;
		auto count = 0;
		for (; block; ++count) {
			block = block->_nextBlock;
		}
		return count;
	};

	std::clog << "Allocator has " << count_blocks() << " blocks" << std::endl;
	for ([[maybe_unused]] auto i : repeat(10)) {
		uint32_t* x = a.allocate(1);
		*x          = -1;
		std::clog << "Allocator has " << count_blocks() << " blocks and " << i + 1 << " elements" << std::endl;
		a.deallocate(x, 1);
	}

	compilable<std::allocator, int>();
	repeatedTest<std::allocator<int>>([] { return std::make_shared<std::allocator<int>>(); });

	compilable<allocator::Mallocator, int>();
	repeatedTest<allocator::Mallocator<int>>([] { return std::make_shared<allocator::Mallocator<int>>(); });

	compilable<allocator::static_size_allocator, int>();
	repeatedTest<allocator::static_size_allocator<int>>([] { return std::make_shared<allocator::static_size_allocator<int>>(); });
	return EXIT_SUCCESS;
}
