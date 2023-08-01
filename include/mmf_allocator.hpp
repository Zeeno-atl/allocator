#pragma once

#include <array>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include <mio/mmap.hpp>

namespace allocator {

/**
 * @brief Memory-mapped file allocator
 * Each allocation is its own file. This is not usefull much on its own, but it can be used
 * in combination with block allocators to create a memory-mapped allocator.
 * It is quite heavy allocator, so you should do as big allocations as possible.
 */

template<class T>
struct mmf_allocator {
	template<typename>
	friend struct mmf_allocator;

	using value_type = T;

	explicit mmf_allocator(std::filesystem::path dir = "") : _p{std::make_shared<ControlBlock>(std::move(dir))} {
	}

	~mmf_allocator() = default;

	mmf_allocator(const mmf_allocator&)                    = default;
	mmf_allocator(mmf_allocator&&)                         = default;
	auto operator=(const mmf_allocator&) -> mmf_allocator& = default;
	auto operator=(mmf_allocator&&) -> mmf_allocator&      = default;

	template<class U>
	constexpr mmf_allocator(const mmf_allocator<U>& other) noexcept : _p{*reinterpret_cast<const decltype(_p)*>(&other._p)} {
	}

	[[nodiscard]] auto allocate(std::size_t n) -> value_type* {
		if (n > std::numeric_limits<std::size_t>::max() / sizeof(value_type)) {
			throw std::bad_array_new_length{};
		}

		std::array<char, 20> buffer{0};
		const auto           result = std::to_chars(buffer.begin(), buffer.end(), _p->_nextId++);
		if (result.ec != std::errc{}) {
			throw std::bad_alloc{};
		}

		std::filesystem::path filename;
		if (_p->_directory.empty()) {
			filename = std::tmpnam(nullptr);
		} else {
			filename = _p->_directory / std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())};
		}
		allocate_file(filename, n * sizeof(value_type));

		std::error_code error;
		mio::mmap_sink  mapping = mio::make_mmap_sink(filename.c_str(), 0, mio::map_entire_file, error);

		if (error) {
			throw std::runtime_error{error.message()};
		}

		auto data = reinterpret_cast<value_type*>(mapping.data());
		_p->_mappings.emplace(data, MappingItem{filename, std::move(mapping)});

		return data;
	}

	void deallocate(value_type* p, [[maybe_unused]] std::size_t n) noexcept {
		_p->_mappings.erase(p);
	}

private:
	static void allocate_file(const std::filesystem::path& filename, std::size_t size) {
		std::ofstream ofs{filename, std::ios::binary | std::ios::out};
		ofs.seekp(static_cast<std::streampos>(size - 1));
		ofs.put(0);
	}

private:
	struct MappingItem {
		std::filesystem::path filename;
		mio::mmap_sink        mapping;

		MappingItem(std::filesystem::path filename, mio::mmap_sink&& mapping) : filename{std::move(filename)}, mapping{std::move(mapping)} {
		}
		MappingItem(const MappingItem&)                        = delete;
		MappingItem(MappingItem&&) noexcept                    = default;
		auto operator=(const MappingItem&) -> MappingItem&     = delete;
		auto operator=(MappingItem&&) noexcept -> MappingItem& = default;
		~MappingItem()                                         = default;
	};
	struct ControlBlock {
		std::atomic_uint_least32_t                   _nextId{0};
		std::filesystem::path                        _directory;
		std::unordered_map<value_type*, MappingItem> _mappings;

		ControlBlock(std::filesystem::path dir) : _directory{std::move(dir)} {
			if (!_directory.empty() && !std::filesystem::exists(_directory)) {
				std::filesystem::create_directories(_directory);
			}
		}

		ControlBlock(const ControlBlock&)                    = delete;
		ControlBlock(ControlBlock&&)                         = delete;
		auto operator=(const ControlBlock&) -> ControlBlock& = delete;
		auto operator=(ControlBlock&&) -> ControlBlock&      = delete;

		~ControlBlock() {
			if (!_directory.empty()) {
				std::filesystem::remove_all(_directory);
			} else {
				for (auto& [_, value] : _mappings) {
					auto            filename{value.filename};
					std::error_code error;
					value.mapping.sync(error);
					value.mapping.unmap();
					std::filesystem::remove(filename);
				}
			}
		}
	};

	std::shared_ptr<ControlBlock> _p;
};

} // namespace allocator
