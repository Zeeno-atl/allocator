#include <chrono>
#include <deque>
#include <format>
#include <forward_list>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "block_adaptor.hpp"
#include "mallocator.hpp"
#include "mmf_allocator.hpp"
#include "round_robin_adaptor.hpp"
#include "universal_block_adaptor.hpp"

#include "pretty_name.hpp"

constexpr auto repeat(int n) {
	return std::views::iota(0, n);
}

void mallocator() {
	std::cout << std::format("{:=^80}", "- mallocator -") << std::endl;
	std::cout << "mallocator class implements a simple allocator that uses malloc and free. According to my tests, it show a slight performance improvement "
	             "over the standard allocator. It is a possible to allocate any number of elements."
	          << std::endl;
	std::cout << std::format("{:=^80}", "- mallocator usage -") << std::endl;

	allocator::mallocator<int> a;

	std::cout << "Allocating 100 ints in a row" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout << "Using this allocator as a vector allocator. This will cause the allocator to allocate bigger and bigger space, as the vector grows."
	          << std::endl;
	std::vector<int, allocator::mallocator<int>> v2{a};
	for (auto i : repeat(100)) {
		v2.push_back(i);
	}
	std::cout << std::format("{:=^80}", "=") << std::endl;
}

void mmf_allocator() {
	std::cout << std::format("{:=^80}", "- mmf_allocator -") << std::endl;
	std::cout << "mmf_allocator class implements a memory mapped file allocator. It is quite a slow allocator, but it can allocate more memory than the "
	             "system has available. It is a possible to allocate any number of elements. Each allocation is its own file. This is not usefull much on its "
	             "own, but in combination with the block_adaptor it can be used to create a very fast memory-mapped allocator. This allocator can be either "
	             "pointed to a directory, or it can create a temporary system file. The directory can be used to create a memory mapped file on a different "
	             "drive, which can be useful for example when you want to create a memory mapped file on a ram drive or a SSD drive."
	          << std::endl;
	std::cout << std::format("{:=^80}", "- mmf_allocator usage -") << std::endl;

	allocator::mmf_allocator<int> a{std::filesystem::current_path() / "mmf"};
	std::cout << "Allocating 10 ints in a row (creating 10 files)" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(10)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout << "Using this allocator as a vector allocator. This will cause the allocator to allocate bigger and bigger space, as the vector grows."
	          << std::endl;
	std::vector<int, allocator::mmf_allocator<int>> v2{a};
	for (auto i : repeat(100)) {
		v2.push_back(i);
	}
	std::cout << std::format("{:=^80}", "=") << std::endl;
}

void block_adaptor() {
	std::cout << std::format("{:=^80}", "- block_adaptor -") << std::endl;
	std::cout
	    << "This adaptor is a limited adaptor that can only allocate a single element and it can not be transformed into an adaptor of any other type. "
	       "In exchange it is very fast and it can be used to allocate a large number of elements. It reuses all the memory it allocates, so it never "
	       "shrinks. Internally it allocates a much bigger block of memory and subsequent calls slice this memory. This make it ideal in combination with a "
	       "memory mapped file allocator. When replacing the standard std::allocator with this allocator adaptor and mallocator, the performance gain is "
	       "usually an order of magnitude."
	    << std::endl;
	std::cout << std::format("{:=^80}", "- block_adaptor usage -") << std::endl;

	allocator::block_adaptor<int> a;

	std::cout << "Allocating 100 ints in a row" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout << "This adaptor can not be used as a vector allocator, since you can only allocate individual elements." << std::endl;
	std::cout << std::format("{:=^80}", "=") << std::endl;
}

void universal_block_adaptor() {
	std::cout << std::format("{:=^80}", "- universal_block_adaptor -") << std::endl;
	std::cout << "This adaptor was developed in order to overcome the limitation of the block_allocator that can not be converted into any adaptor of "
	             "a different type. Internally it uses a series of block_adaptor to allocate memory of different sizes. It can potentially waste up "
	             "to 50% of the space if the element size is n^2+1. It is still much faster than the standard allocator, but slower than the "
	             "block_adaptor, yet it can allocate only 1 element at a time. This limits the usage, since it can not be used for a vector, but can "
	             "be used for a shared_ptr for example."
	          << std::endl;
	std::cout << std::format("{:=^80}", "- universal_block_adaptor usage -") << std::endl;

	allocator::universal_block_adaptor<int> a;

	std::cout << "Allocating 100 ints in a row" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout << "Transforming the allocate to a adaptor that can allocate different size elements" << std::endl;
	std::allocator_traits<decltype(a)>::rebind_alloc<double> a2{a};

	std::cout << "Allocating 100 doubles in a row" << std::endl;
	std::vector<double*> v2;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a2.allocate(1);
		v2.push_back(p);
	}
	for (auto p : v2) {
		a2.deallocate(p, 1);
	}

	std::cout << "This adaptor can not be used as a vector allocator, since you can only allocate individual elements." << std::endl;
	std::cout << std::format("{:=^80}", "=") << std::endl;
}

void round_robin_adaptor() {
	std::cout << std::format("{:=^80}", "- round_robin_adaptor -") << std::endl;
	std::cout << "This adaptor is kinda a special purpose adaptor. It makes sense only when you have multiple slower allocators and you want to "
	             "distribute the load between them. The typical use would be in combination with the block_adaptor and mmf_allocator."
	          << std::endl;
	std::cout << std::format("{:=^80}", "- round_robin_adaptor usage -") << std::endl;

	using Alloc = allocator::round_robin_adaptor<int, allocator::mallocator<int>, std::allocator<int>>;
	Alloc a{allocator::mallocator<int>{}, std::allocator<int>{}};

	std::cout << "Allocating 100 ints in a row, alternating between mallocator and std::allocator" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout
	    << "This adaptor inherits the limitations of individual allocators. In the case of using it with std::allocator and allocator::mallocator, it can "
	       "be used as vector allocator."
	    << std::endl;
	std::vector<int, Alloc> v2{a};
	for (auto i : repeat(100)) {
		v2.push_back(i);
	}
	std::cout << std::format("{:=^80}", "=") << std::endl;
}

void ultimate_infinite_capacity_speed() {
	std::cout << std::format("{:=^80}", "- ultimate_infinite_capacity_speed -") << std::endl;
	std::cout << "By combining allocators and adaptors you can achieve various behaviours. Once at my job we had a big challenge to cache data from detectors "
	             "that compressed saturated 10Gbps link and we had to process it. The obvious solution here is to use memory mapped files for individual "
	             "objects sent over the network. But at that time even fast SSDs could not keep up after filling in the SLC cache. So we used a several SSDs, "
	             "but on a consumer grade motherboard and with non-server Windows edition it was nontrivial to make proper performance-boosting RAID. This "
	             "project would allow us to use several mmf_allocators wrapped in block_adaptors and round_robin_adaptor to distribute the load between the "
	             "SSDs. We did just that, but the solution was much more complex than simply creating allocator in this modular way."
	          << std::endl;

	using DiskAllocator = allocator::block_adaptor<int, 4 * 1024 * 1024, allocator::mmf_allocator>;
	using Alloc         = allocator::round_robin_adaptor<int, DiskAllocator, DiskAllocator, DiskAllocator, DiskAllocator>;
	Alloc
	    a{DiskAllocator{allocator::mmf_allocator<int>{std::filesystem::current_path() / "mmf1"}},
	      DiskAllocator{allocator::mmf_allocator<int>{std::filesystem::current_path() / "mmf2"}},
	      DiskAllocator{allocator::mmf_allocator<int>{std::filesystem::current_path() / "mmf3"}},
	      DiskAllocator{allocator::mmf_allocator<int>{std::filesystem::current_path() / "mmf4"}}};

	std::cout << "Allocating 100 ints in a row, alternating between 4 mmf_allocators wrapped in a block_adaptor" << std::endl;
	std::vector<int*> v;
	for ([[maybe_unused]] auto i : repeat(100)) {
		auto p = a.allocate(1);
		v.push_back(p);
	}
	for (auto p : v) {
		a.deallocate(p, 1);
	}

	std::cout << std::format("{:=^80}", "=") << std::endl;
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char const* argv[]) -> int {
	try {
		mallocator();
		mmf_allocator();
		block_adaptor();
		universal_block_adaptor();
		round_robin_adaptor();
		ultimate_infinite_capacity_speed();

		return EXIT_SUCCESS;
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unknown exception" << std::endl;
	}
	return EXIT_FAILURE;
}
