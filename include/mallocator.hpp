#pragma once

#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace allocator {

template<class T>
struct mallocator {
	using value_type = T;

	mallocator() = default;

	template<class U>
	constexpr explicit mallocator(const mallocator<U>&) noexcept {
	}

	[[nodiscard]] auto allocate(std::size_t n) -> T* {
		if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
			throw std::bad_array_new_length{};
		}

		if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
			return p;
		}

		throw std::bad_alloc{};
	}

	void deallocate(T* p, [[maybe_unused]] std::size_t n) noexcept {
		std::free(p);
	}
};

template<class T, class U>
auto operator==(const mallocator<T>&, const mallocator<U>&) -> bool {
	return true;
}

template<class T, class U>
auto operator!=(const mallocator<T>&, const mallocator<U>&) -> bool {
	return false;
}

} // namespace allocator
