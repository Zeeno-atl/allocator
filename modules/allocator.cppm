module;

#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

export module allocator;

export namespace allocator {

template<class T>
struct Mallocator {
	using value_type = T;

	Mallocator() = default;

	template<class U>
	constexpr Mallocator(const Mallocator<U>&) noexcept {
	}

	[[nodiscard]] T* allocate(std::size_t n) {
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
bool operator==(const Mallocator<T>&, const Mallocator<U>&) {
	return true;
}

template<class T, class U>
bool operator!=(const Mallocator<T>&, const Mallocator<U>&) {
	return false;
}

/**
 * 
*/

template<class T>
struct static_size_allocator {
	constexpr static const std::size_t ELEM_SIZE{std::max(sizeof(T), sizeof(void*))};

	struct Block {
		std::byte*        _data{nullptr};
		const std::size_t _size{};
		void*             _free{nullptr};
		Block*            _nextBlock{nullptr};

		Block(std::size_t blockSize) : _size{blockSize} {
			_data = new std::byte[blockSize];
			std::size_t i{0};
			for (; i <= blockSize - ELEM_SIZE; i += ELEM_SIZE) {
				*reinterpret_cast<void**>(_data + i) = static_cast<void*>(_data + i + ELEM_SIZE);
			}
			*reinterpret_cast<void**>(_data + i - ELEM_SIZE) = nullptr;
			_free                                            = _data;
		}

		Block(const Block&)            = delete;
		Block& operator=(const Block&) = delete;
		Block(Block&&)                 = default;
		Block& operator=(Block&&)      = default;

		void* take() {
			if (_free == nullptr) {
				return nullptr;
			}
			void* p = _free;
			_free   = *static_cast<void**>(_free);
			return p;
		}

		bool give(void* p) {
			if (p < _data || p >= _data + _size) {
				return false;
			}
			*static_cast<void**>(p) = _free;
			_free                   = p;
			return true;
		}

		~Block() {
			delete[] _data;
		}
	};

	using value_type = T;

	static_size_allocator() = default;
	static_size_allocator(std::size_t blockSize) : _blockSize{blockSize} {
	}
	~static_size_allocator() {
		while (_firstBlock) {
			auto nextBlock = _firstBlock->_nextBlock;
			delete _firstBlock;
			_firstBlock = nextBlock;
		}
	}

	[[nodiscard]] T* allocate(std::size_t n) {
		if (n - 1) {
			throw std::bad_array_new_length();
		}

		Block* block{_firstBlock};
		Block* lastBlock{nullptr};
		while (block) {
			if (auto p = static_cast<T*>(block->take())) {
				return p;
			}
			lastBlock = block;
			block     = block->_nextBlock;
		}

		auto newBlock        = new Block(_blockSize);
		newBlock->_nextBlock = _firstBlock;
		_firstBlock          = newBlock;

		if (auto p = static_cast<T*>(newBlock->take())) {
			return p;
		}

		throw std::bad_alloc();
	}

	void deallocate(T* p, std::size_t n) noexcept {
		if (n - 1) {
			return;
		}

		auto block{_firstBlock};
		while (block) {
			if (block->give(p)) {
				return;
			}
			block = block->_nextBlock;
		}
	}

	//private:
	const std::size_t _blockSize{4096 * 1024};
	Block*            _firstBlock{nullptr};
};

template<typename T, typename U>
bool operator==(const static_size_allocator<T>&, const static_size_allocator<U>&) {
	return false;
}

template<typename T, typename U>
bool operator!=(const static_size_allocator<T>&, const static_size_allocator<U>&) {
	return true;
}

} // namespace allocator
