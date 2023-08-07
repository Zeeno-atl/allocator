#pragma once

#include "dummy_mutex.hpp"
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace allocator {

template<typename T, typename Mutex = dummy_mutex, typename... Allocs>
class round_robin_adaptor {
public:
	using value_type = T;

	round_robin_adaptor(Allocs&&... allocs) : _p{std::make_shared<ControlBlock>(std::forward<Allocs>(allocs)...)} {
	}

	~round_robin_adaptor() = default;

	round_robin_adaptor(const round_robin_adaptor&)                    = default;
	round_robin_adaptor(round_robin_adaptor&&)                         = default;
	auto operator=(const round_robin_adaptor&) -> round_robin_adaptor& = default;
	auto operator=(round_robin_adaptor&&) -> round_robin_adaptor&      = default;

	[[nodiscard]] auto allocate(std::size_t n) -> value_type* {
		return allocateImpl(next(), n);
	}

	void deallocate(value_type* p, std::size_t n) {
		decltype(_p->allocations.find(p)) it;
		{
			std::scoped_lock lock{_p->mutex};
			it = _p->allocations.find(p);
			if (it == _p->allocations.end()) {
				throw std::invalid_argument{"Pointer was not allocated by this allocator"};
			}
		}

		deallocateImpl(it->second, p, n);
	}

private:
	[[nodiscard]] auto next() -> std::size_t {
		return _p->next++ % sizeof...(Allocs);
	}

	template<std::size_t Index = 0>
	void deallocateImpl(std::size_t allocNo, value_type* p, std::size_t n) {
		if constexpr (Index >= sizeof...(Allocs)) {
			throw std::out_of_range{"Index out of range"};
		} else {
			if (Index == allocNo) {
				std::get<Index>(_p->allocs).deallocate(p, n);
				{
					std::scoped_lock lock{_p->mutex};
					_p->allocations.erase(p);
				}
			} else {
				deallocateImpl<Index + 1>(allocNo, p, n);
			}
		}
	}

	template<std::size_t Index = 0>
	[[nodiscard]] auto allocateImpl(std::size_t allocNo, std::size_t n) -> value_type* {
		if constexpr (Index >= sizeof...(Allocs)) {
			throw std::out_of_range{"Index out of range"};
		} else {
			if (Index == allocNo) {
				auto ptr = std::get<Index>(_p->allocs).allocate(n);
				{
					std::scoped_lock lock{_p->mutex};
					_p->allocations[ptr] = Index;
				}
				return ptr;
			} else {
				return allocateImpl<Index + 1>(allocNo, n);
			}
		}
	}

private:
	struct ControlBlock {
		std::tuple<Allocs...>                        allocs;
		std::atomic_uint_least32_t                   next{0};
		std::unordered_map<value_type*, std::size_t> allocations;
		Mutex                                        mutex;

		ControlBlock(Allocs&&... allocs) : allocs{allocs...} {
		}
	};
	std::shared_ptr<ControlBlock> _p;
};

} // namespace allocator
