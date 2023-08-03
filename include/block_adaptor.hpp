#pragma once

#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace allocator {

template<std::size_t ELEM_SIZE, std::size_t BLOCK_SIZE, template<typename...> typename Alloc = std::allocator>
struct block_adaptor_block {
	using byte_type  = std::byte;
	using alloc_type = Alloc<byte_type>;
	byte_type*           _data{nullptr};
	void*                _free{nullptr};
	block_adaptor_block* _nextBlock{nullptr};
	alloc_type           _alloc{};

	explicit block_adaptor_block(alloc_type alloc) : _alloc{alloc} {
		_data = std::allocator_traits<alloc_type>::allocate(_alloc, BLOCK_SIZE);

		std::size_t i{0};
		for (; i <= BLOCK_SIZE - ELEM_SIZE; i += ELEM_SIZE) {
			*reinterpret_cast<void**>(_data + i) = static_cast<void*>(_data + i + ELEM_SIZE);
		}
		*reinterpret_cast<void**>(_data + i - ELEM_SIZE) = nullptr;
		_free                                            = _data;
	}

	block_adaptor_block(const block_adaptor_block&)                    = delete;
	auto operator=(const block_adaptor_block&) -> block_adaptor_block& = delete;
	block_adaptor_block(block_adaptor_block&&)                         = default;
	auto operator=(block_adaptor_block&&) -> block_adaptor_block&      = default;

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

	~block_adaptor_block() {
		std::allocator_traits<alloc_type>::deallocate(_alloc, _data, BLOCK_SIZE);
	}
};

template<typename T, std::size_t BLOCK_SIZE = 4 * 1024 * 1024, template<typename...> typename Alloc = std::allocator>
struct block_adaptor {
	constexpr static const std::size_t ELEM_SIZE{std::max(sizeof(T), sizeof(void*))};

	using value_type = T;
	using alloc_type = Alloc<block_adaptor_block<std::max(sizeof(T), sizeof(void*)), BLOCK_SIZE, Alloc>>;
	using Block      = block_adaptor_block<ELEM_SIZE, BLOCK_SIZE, Alloc>;

	struct ControlBlock {
		alloc_type _alloc{};
		Block*     _firstBlock{};

		explicit ControlBlock(alloc_type&& alloc) : _alloc{alloc} {
		}

		~ControlBlock() {
			while (_firstBlock) {
				auto nextBlock = _firstBlock->_nextBlock;
				std::allocator_traits<alloc_type>::destroy(_alloc, _firstBlock);
				std::allocator_traits<alloc_type>::deallocate(_alloc, _firstBlock, 1);
				_firstBlock = nextBlock;
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

		Block* block{_controlBlock->_firstBlock};
		while (block) {
			if (auto p = static_cast<value_type*>(block->take())) {
				return p;
			}
			block = block->_nextBlock;
		}

		auto newBlock = std::allocator_traits<alloc_type>::allocate(_controlBlock->_alloc, 1);
		std::allocator_traits<alloc_type>::construct(_controlBlock->_alloc, newBlock, Alloc<std::byte>{_controlBlock->_alloc});
		newBlock->_nextBlock       = _controlBlock->_firstBlock;
		_controlBlock->_firstBlock = newBlock;

		if (auto p = static_cast<value_type*>(newBlock->take())) {
			return p;
		}

		throw std::bad_alloc();
	}

	void deallocate(value_type* p, std::size_t n) noexcept {
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
