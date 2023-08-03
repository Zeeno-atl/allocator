#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <memory>

namespace allocator {

/*
	This universal allocator has a series of allocators for different sizes of objects.
	It uses the smallest allocator that can fit the object.
	It is not thread safe.
	It starts with objects of size sizeof(void*) bytes (8B on 64-bit systems) and doubles the size
	until it reaches the number of blocks specified in the template parameter.
*/
template<typename T = std::byte, std::size_t SUBALLOCATORS = 6, std::size_t BLOCK_SIZE = 4 * 1024 * 1024, template<typename...> typename Alloc = std::allocator>
struct universal_block_adaptor {
	template<typename, std::size_t, std::size_t, template<typename...> typename>
	friend struct universal_block_adaptor;

	using value_type = T;

	template<typename U>
	struct rebind {
		using other = universal_block_adaptor<U, SUBALLOCATORS>;
	};

	universal_block_adaptor() : _alloc{std::make_shared<allocator_tuple_type>()} {
	}

	template<typename U = void>
	explicit universal_block_adaptor(const universal_block_adaptor<U, SUBALLOCATORS>& other)
	    : _alloc{*reinterpret_cast<const decltype(_alloc)*>(&other._alloc)} {
	}

	template<typename U, typename... Args>
	auto allocate_shared(Args&&... args) -> std::shared_ptr<U> {
		return std::allocate_shared<U>(*this, std::forward<Args>(args)...);
	}

	[[nodiscard]] auto allocate(std::size_t n) -> value_type* {
		return allocator<value_type>().allocate(n);
	}

	void deallocate(value_type* p, std::size_t n) noexcept {
		allocator<value_type>().deallocate(p, n);
	}

private:
	template<std::size_t ObjectSize>
	struct Filler {
		std::array<std::byte, ObjectSize> _data;
		static_assert(sizeof(_data) == ObjectSize, "Filler size is not correct");
	};

	static constexpr auto cellSize(std::size_t bytes) -> std::size_t {
		return std::max(std::bit_ceil(bytes), sizeof(void*));
	}

	template<typename U>
	static constexpr auto cellSize() -> std::size_t {
		return cellSize(sizeof(U));
	}

	static constexpr auto posFromSize(std::size_t size) -> std::size_t {
		return std::bit_width(cellSize(size)) - std::bit_width(cellSize(0));
	}

	template<typename U>
	static constexpr auto posForType() -> std::size_t {
		return posFromSize(sizeof(U));
	}

	static constexpr auto blockSizeForCellSize(std::size_t size) -> std::size_t {
		constexpr std::size_t minElements{1024};
		const std::size_t     elements{std::max(BLOCK_SIZE / size, minElements)};
		return elements * size;
	}

	template<typename U>
	using filler_allocator_type = block_adaptor<Filler<cellSize(sizeof(U))>, BLOCK_SIZE, Alloc>;

	template<typename U>
	auto allocator() -> block_adaptor<U, BLOCK_SIZE, Alloc>& {
		static_assert(posForType<U>() < SUBALLOCATORS, "type too big for a allocator");
		return *reinterpret_cast<block_adaptor<U, BLOCK_SIZE, Alloc>*>(&std::get<posForType<U>()>(*_alloc));
	}

	template<std::size_t... Index>
	static auto helper(std::index_sequence<Index...>) {
		return std::tuple<block_adaptor<Filler<cellSize(1 << (std::bit_width(sizeof(void*)) + Index - 1))>, BLOCK_SIZE, Alloc>...>{};
	}

	using allocator_tuple_type = decltype(helper(std::make_index_sequence<SUBALLOCATORS>{}));

private:
	std::shared_ptr<allocator_tuple_type> _alloc;
};

template<typename T, typename U, std::size_t SUBALLOCATORS>
auto operator==(const universal_block_adaptor<T, SUBALLOCATORS>& a, const universal_block_adaptor<U, SUBALLOCATORS>& b) -> bool {
	return a._alloc == b._alloc;
}

template<typename T, typename U, std::size_t SUBALLOCATORS>
auto operator!=(const universal_block_adaptor<T, SUBALLOCATORS>& a, const universal_block_adaptor<U, SUBALLOCATORS>& b) -> bool {
	return a._alloc != b._alloc;
}

} // namespace allocator
