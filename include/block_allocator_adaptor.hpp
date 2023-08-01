#pragma once

#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace allocator {

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

} // namespace allocator
