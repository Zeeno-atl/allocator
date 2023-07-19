#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace allocator {

template<class T>
struct mallocator {
	using value_type = T;

	mallocator() = default;

	template<class U>
	constexpr explicit mallocator(const mallocator<U>&) noexcept {
	}

	[[nodiscard]] auto allocate(std::size_t n) -> T* {
		if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
			throw std::bad_array_new_length();

		if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
			return p;
		}

		throw std::bad_alloc();
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

/**
 * 
*/

template<std::size_t ELEM_SIZE, std::size_t BLOCK_SIZE, template<typename...> typename Alloc = std::allocator>
struct block_allocator_adaptor_block {
	using byte_type  = std::byte;
	using alloc_type = Alloc<byte_type>;
	byte_type*                     _data{nullptr};
	void*                          _free{nullptr};
	block_allocator_adaptor_block* _nextBlock{nullptr};
	alloc_type                     _alloc{};

	explicit block_allocator_adaptor_block(alloc_type alloc) : _alloc{alloc} {
		_data = std::allocator_traits<alloc_type>::allocate(_alloc, BLOCK_SIZE);

		std::size_t i{0};
		for (; i <= BLOCK_SIZE - ELEM_SIZE; i += ELEM_SIZE) {
			*reinterpret_cast<void**>(_data + i) = static_cast<void*>(_data + i + ELEM_SIZE);
		}
		*reinterpret_cast<void**>(_data + i - ELEM_SIZE) = nullptr;
		_free                                            = _data;
	}

	block_allocator_adaptor_block(const block_allocator_adaptor_block&)                    = delete;
	auto operator=(const block_allocator_adaptor_block&) -> block_allocator_adaptor_block& = delete;
	block_allocator_adaptor_block(block_allocator_adaptor_block&&)                         = default;
	auto operator=(block_allocator_adaptor_block&&) -> block_allocator_adaptor_block&      = default;

	auto take() -> void* {
		if (_free == nullptr) {
			return nullptr;
		}
		void* p = _free;
		_free   = *static_cast<void**>(_free);
		return p;
	}

	auto give(void* p) -> bool {
		if (p < _data || p >= _data + BLOCK_SIZE) {
			return false;
		}
		*static_cast<void**>(p) = _free;
		_free                   = p;
		return true;
	}

	~block_allocator_adaptor_block() {
		std::allocator_traits<alloc_type>::deallocate(_alloc, _data, BLOCK_SIZE);
	}
};

template<typename T, std::size_t BLOCK_SIZE = 4 * 1024 * 1024, template<typename...> typename Alloc = std::allocator>
struct block_allocator_adaptor {
	constexpr static const std::size_t ELEM_SIZE{std::max(sizeof(T), sizeof(void*))};

	using value_type = T;
	using alloc_type = Alloc<block_allocator_adaptor_block<std::max(sizeof(T), sizeof(void*)), BLOCK_SIZE, Alloc>>;
	using Block      = block_allocator_adaptor_block<ELEM_SIZE, BLOCK_SIZE, Alloc>;

	struct ControlBlock {
		alloc_type _alloc{};
		Block*     _firstBlock{};
	};
	using control_block_alloc_type = typename std::allocator_traits<alloc_type>::template rebind_alloc<ControlBlock>;

	block_allocator_adaptor(alloc_type alloc = alloc_type()) : _controlBlockAlloc{alloc} {
		_controlBlock = _controlBlockAlloc.allocate(1);
		std::allocator_traits<control_block_alloc_type>::construct(_controlBlockAlloc, _controlBlock);
		_controlBlock->_alloc = alloc;
	}

	block_allocator_adaptor(const block_allocator_adaptor&) = default;
	block_allocator_adaptor(block_allocator_adaptor&&)      = default;

	auto operator=(const block_allocator_adaptor& other) -> block_allocator_adaptor& {
		if (&other == this) {
			return *this;
		}

		deallocateAll();
		_controlBlock      = other._controlBlock;
		_controlBlockAlloc = other._controlBlockAlloc;
		return *this;
	}

	auto operator=(block_allocator_adaptor&& other) noexcept -> block_allocator_adaptor& {
		deallocateAll();
		std::swap(_controlBlock, other._controlBlock);
		std::swap(_controlBlockAlloc, other._controlBlockAlloc);
		return *this;
	}

	~block_allocator_adaptor() {
		deallocateAll();
	}

	[[nodiscard]] auto allocate(std::size_t n) -> T* {
		if (n - 1) {
			throw std::bad_array_new_length();
		}

		Block* block{_controlBlock->_firstBlock};
		while (block) {
			if (auto p = static_cast<T*>(block->take())) {
				return p;
			}
			block = block->_nextBlock;
		}

		auto newBlock = std::allocator_traits<alloc_type>::allocate(_controlBlock->_alloc, 1);
		std::allocator_traits<alloc_type>::construct(_controlBlock->_alloc, newBlock, Alloc<std::byte>{_controlBlock->_alloc});
		newBlock->_nextBlock       = _controlBlock->_firstBlock;
		_controlBlock->_firstBlock = newBlock;

		if (auto p = static_cast<T*>(newBlock->take())) {
			return p;
		}

		throw std::bad_alloc();
	}

	void deallocate(T* p, std::size_t n) noexcept {
		if (n - 1) {
			return;
		}

		auto block{_controlBlock->_firstBlock};
		while (block) {
			if (block->give(p)) {
				return;
			}
			block = block->_nextBlock;
		}
	}

	void deallocateAll() {
		while (_controlBlock->_firstBlock) {
			auto nextBlock = _controlBlock->_firstBlock->_nextBlock;
			std::allocator_traits<alloc_type>::destroy(_controlBlock->_alloc, _controlBlock->_firstBlock);
			std::allocator_traits<alloc_type>::deallocate(_controlBlock->_alloc, _controlBlock->_firstBlock, 1);
			_controlBlock->_firstBlock = nextBlock;
		}
		std::allocator_traits<control_block_alloc_type>::destroy(_controlBlockAlloc, _controlBlock);
		std::allocator_traits<control_block_alloc_type>::deallocate(_controlBlockAlloc, _controlBlock, 1);
		_controlBlock = nullptr;
	}

private:
	ControlBlock*            _controlBlock{};
	control_block_alloc_type _controlBlockAlloc{};
};

template<typename T, typename U>
auto operator==(const block_allocator_adaptor<T>&, const block_allocator_adaptor<U>&) -> bool {
	return false;
}

template<typename T, typename U>
auto operator!=(const block_allocator_adaptor<T>&, const block_allocator_adaptor<U>&) -> bool {
	return true;
}

/*
	This universal allocator has a series of allocators for different sizes of objects.
	It uses the smallest allocator that can fit the object.
	It is not thread safe.
	It starts with objects of size sizeof(void*) bytes (8B on 64-bit systems) and doubles the size
	until it reaches the number of blocks specified in the template parameter.
*/
template<typename T = std::byte, std::size_t SUBALLOCATORS = 6, std::size_t BLOCK_SIZE = 4 * 1024 * 1024, template<typename...> typename Alloc = std::allocator>
struct universal_allocator {
	template<typename, std::size_t, std::size_t, template<typename...> typename>
	friend struct universal_allocator;

	using value_type = T;

	template<typename U>
	struct rebind {
		using other = universal_allocator<U, SUBALLOCATORS>;
	};

	//template<size_t

	universal_allocator() : _alloc(std::make_shared<allocator_tuple_type>()) {
	}

	template<typename U = void>
	explicit universal_allocator(const universal_allocator<U, SUBALLOCATORS>& other) : _alloc{*reinterpret_cast<const decltype(_alloc)*>(&other._alloc)} {
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
	using filler_allocator_type = block_allocator_adaptor<Filler<cellSize(sizeof(U))>, BLOCK_SIZE, Alloc>;

	template<typename U>
	auto allocator() -> block_allocator_adaptor<U, BLOCK_SIZE, Alloc>& {
		static_assert(posForType<U>() < SUBALLOCATORS, "type too big for a allocator");
		return *reinterpret_cast<block_allocator_adaptor<U, BLOCK_SIZE, Alloc>*>(&std::get<posForType<U>()>(*_alloc));
	}

	template<std::size_t... Index>
	static auto helper(std::index_sequence<Index...>) {
		return std::tuple<block_allocator_adaptor<Filler<cellSize(1 << (std::bit_width(sizeof(void*)) + Index - 1))>, BLOCK_SIZE, Alloc>...>{};
	}

	using allocator_tuple_type = decltype(helper(std::make_index_sequence<SUBALLOCATORS>{}));

private:
	std::shared_ptr<allocator_tuple_type> _alloc;
};

template<typename T, typename U, std::size_t SUBALLOCATORS>
auto operator==(const universal_allocator<T, SUBALLOCATORS>& a, const universal_allocator<U, SUBALLOCATORS>& b) -> bool {
	return a._allocators == b._allocators;
}

template<typename T, typename U, std::size_t SUBALLOCATORS>
auto operator!=(const universal_allocator<T, SUBALLOCATORS>& a, const universal_allocator<U, SUBALLOCATORS>& b) -> bool {
	return a._allocators != b._allocators;
}

} // namespace allocator
