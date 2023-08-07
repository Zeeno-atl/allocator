#pragma once

#include "dummy_mutex.hpp"
#include <array>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace allocator {

template<typename T, std::size_t BLOCK_SIZE = 4 * 1024 * 1024, template<typename...> typename Alloc = std::allocator, typename Mutex = dummy_mutex>
struct block_adaptor {
	constexpr static const std::size_t ELEM_SIZE{std::max(sizeof(T), sizeof(void*))};

	struct Block : Mutex {
		using byte_type       = std::byte;
		using byte_alloc_type = Alloc<byte_type>;

		byte_type*      _data{nullptr};
		void*           _free{nullptr};
		Block*          _nextBlock{nullptr};
		byte_alloc_type _alloc{};

		explicit Block(byte_alloc_type alloc) : _alloc{alloc} {
			std::scoped_lock lock{*this};
			_data = std::allocator_traits<byte_alloc_type>::allocate(_alloc, BLOCK_SIZE);

			std::size_t i{0};
			for (; i <= BLOCK_SIZE - ELEM_SIZE; i += ELEM_SIZE) {
				*reinterpret_cast<void**>(_data + i) = static_cast<void*>(_data + i + ELEM_SIZE);
			}
			*reinterpret_cast<void**>(_data + i - ELEM_SIZE) = nullptr;
			_free                                            = _data;
		}

		Block(const Block&)                    = delete;
		auto operator=(const Block&) -> Block& = delete;
		Block(Block&&)                         = default;
		auto operator=(Block&&) -> Block&      = default;

		auto take() -> void* {
			std::scoped_lock lock{*this};
			if (_free == nullptr) {
				return nullptr;
			}
			void* p = _free;
			_free   = *static_cast<void**>(_free);
			return p;
		}

		auto give(void* p) -> bool {
			std::scoped_lock lock{*this};
			if (p < _data || p >= _data + BLOCK_SIZE) {
				return false;
			}
			*static_cast<void**>(p) = _free;
			_free                   = p;
			return true;
		}

		~Block() {
			std::scoped_lock lock{*this};
			std::allocator_traits<byte_alloc_type>::deallocate(_alloc, _data, BLOCK_SIZE);
		}
	};

	using value_type = T;
	using alloc_type = Alloc<Block>;

	struct ControlBlock : Mutex {
		alloc_type alloc{};
		Block*     firstBlock{};

		explicit ControlBlock(alloc_type&& alloc) : alloc{alloc} {
		}

		~ControlBlock() {
			std::scoped_lock lock{*this};
			while (firstBlock) {
				auto nextBlock = firstBlock->_nextBlock;
				std::allocator_traits<alloc_type>::destroy(alloc, firstBlock);
				std::allocator_traits<alloc_type>::deallocate(alloc, firstBlock, 1);
				firstBlock = nextBlock;
			}
		}
	};

	block_adaptor(alloc_type&& alloc = alloc_type()) : _controlBlock{std::make_shared<ControlBlock>(std::move(alloc))} {
	}

	block_adaptor(const block_adaptor&)                    = default;
	block_adaptor(block_adaptor&&)                         = default;
	auto operator=(const block_adaptor&) -> block_adaptor& = default;
	auto operator=(block_adaptor&&) -> block_adaptor&      = default;

	~block_adaptor() {
	}

	[[nodiscard]] auto allocate(std::size_t n) -> value_type* {
		if (n - 1) {
			throw std::bad_array_new_length();
		}

		Block* block;
		{
			std::scoped_lock lock{*_controlBlock};
			block = _controlBlock->firstBlock;
		}
		while (block) {
			if (auto p = static_cast<value_type*>(block->take()); p) {
				return p;
			}
			{
				std::scoped_lock lock{*block};
				block = block->_nextBlock;
			}
		}

		block = std::allocator_traits<alloc_type>::allocate(_controlBlock->alloc, 1);
		std::allocator_traits<alloc_type>::construct(_controlBlock->alloc, block, Alloc<std::byte>{_controlBlock->alloc});
		{
			std::scoped_lock lock{*_controlBlock, *block};
			block->_nextBlock         = _controlBlock->firstBlock;
			_controlBlock->firstBlock = block;
		}

		if (auto p = static_cast<value_type*>(block->take()); p) {
			return p;
		}

		throw std::bad_alloc();
	}

	void deallocate(value_type* p, std::size_t n) noexcept {
		if (n - 1) {
			return;
		}

		Block* block;
		{
			std::scoped_lock lock{*_controlBlock};
			block = _controlBlock->firstBlock;
		}
		while (block) {
			if (block->give(p)) {
				return;
			}
			{
				std::scoped_lock lock{*block};
				block = block->_nextBlock;
			}
		}
	}

private:
	std::shared_ptr<ControlBlock> _controlBlock;
};

template<typename T, typename U>
auto operator==(const block_adaptor<T>&, const block_adaptor<U>&) -> bool {
	return false;
}

template<typename T, typename U>
auto operator!=(const block_adaptor<T>&, const block_adaptor<U>&) -> bool {
	return true;
}

} // namespace allocator
