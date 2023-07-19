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

#include "allocator.hpp"
#include "pretty_name.hpp"

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
	[[maybe_unused]] std::vector<T, Alloc<T>>                                                                   v;
	[[maybe_unused]] std::map<T, T, std::less<>, Alloc<std::pair<const T, T>>>                                  m;
	[[maybe_unused]] std::set<T, std::less<>, Alloc<T>>                                                         s;
	[[maybe_unused]] std::unordered_map<T, T, std::hash<T>, std::equal_to<>, Alloc<std::pair<const T, T>>>      um;
	[[maybe_unused]] std::unordered_set<T, std::hash<T>, std::equal_to<>, Alloc<T>>                             us;
	[[maybe_unused]] std::multimap<T, T, std::less<>, Alloc<std::pair<const T, T>>>                             mm;
	[[maybe_unused]] std::multiset<T, std::less<>, Alloc<T>>                                                    ms;
	[[maybe_unused]] std::unordered_multimap<T, T, std::hash<T>, std::equal_to<>, Alloc<std::pair<const T, T>>> umm;
	[[maybe_unused]] std::unordered_multiset<T, std::hash<T>, std::equal_to<>, Alloc<T>>                        ums;
	[[maybe_unused]] std::deque<T, Alloc<T>>                                                                    d;
	[[maybe_unused]] std::list<T, Alloc<T>>                                                                     l;
	[[maybe_unused]] std::forward_list<T, Alloc<T>>                                                             fl;
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
	try {
		{
			allocator::universal_allocator a;
			auto                           shared_int{a.allocate_shared<int>(1)};
			auto                           shared_double{a.allocate_shared<double>(2.0)};
			auto                           shared_longdouble{a.allocate_shared<long double>(3.0L)};
			std::cout << *shared_int << '\n';
			std::cout << *shared_double << '\n';
			std::cout << *shared_longdouble << '\n';
		}
		compilable<std::allocator, int>();
		repeatedTest<std::allocator<int>>([] { return std::make_shared<std::allocator<int>>(); });

		compilable<allocator::mallocator, int>();
		repeatedTest<allocator::mallocator<int>>([] { return std::make_shared<allocator::mallocator<int>>(); });

		repeatedTest<allocator::universal_allocator<int>>([] { return std::make_shared<allocator::universal_allocator<int>>(); });
		{
			using T = int;

			std::vector<T, allocator::universal_allocator<T>>                                  v;
			std::map<T, T, std::less<>, allocator::universal_allocator<std::pair<const T, T>>> m;
			std::set<T, std::less<>, allocator::universal_allocator<T>>                        s;
#ifndef _MSC_VER
			std::unordered_map<T, T, std::hash<T>, std::equal_to<>, allocator::universal_allocator<std::pair<const T, T>>> um;
			std::unordered_set<T, std::hash<T>, std::equal_to<>, allocator::universal_allocator<T>>                        us;
#endif
			std::multimap<T, T, std::less<>, allocator::universal_allocator<std::pair<const T, T>>> mm;
			std::multiset<T, std::less<>, allocator::universal_allocator<T>>                        ms;
#ifndef _MSC_VER
			std::unordered_multimap<T, T, std::hash<T>, std::equal_to<>, allocator::universal_allocator<std::pair<const T, T>>> umm;
			std::unordered_multiset<T, std::hash<T>, std::equal_to<>, allocator::universal_allocator<T>>                        ums;
#endif
#ifndef __GLIBCXX__
			std::deque<T, allocator::universal_allocator<T>> d;
#endif
			std::list<T, allocator::universal_allocator<T>>         l;
			std::forward_list<T, allocator::universal_allocator<T>> fl;
		}

		using better_universal_allocator = allocator::universal_allocator<int, 6, 4ul * 1024 * 1024, allocator::mallocator>;
		repeatedTest<better_universal_allocator>([] { return std::make_shared<better_universal_allocator>(); });

		repeatedTest<allocator::block_allocator_adaptor<int>>([] { return std::make_shared<allocator::block_allocator_adaptor<int>>(); });

		using better_block_allocator = allocator::block_allocator_adaptor<int, 4ul * 1024 * 1024, allocator::mallocator>;
		repeatedTest<better_block_allocator>([] { return std::make_shared<better_block_allocator>(); });
		return EXIT_SUCCESS;
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
	} catch (...) {
	}
	return EXIT_FAILURE;
}
