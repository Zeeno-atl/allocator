#pragma once

/**
 * @file pretty_name.hpp
 * @author Ondřej Beňuš & some person on stackoverflow.com I can not find anymore
 * @brief An easy way to get pretty names for types.
 * @date 2021-12-26
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

class static_string {
	const char* const p_;
	const std::size_t sz_;

public:
	typedef const char* const_iterator;

	template<std::size_t N>
	constexpr explicit static_string(const char (&a)[N]) noexcept : p_(a), sz_(N - 1) {
	}
	constexpr static_string(const char* p, std::size_t N) noexcept : p_(p), sz_(N) {
	}
	constexpr const char* data() const noexcept {
		return p_;
	}
	constexpr std::size_t size() const noexcept {
		return sz_;
	}
	constexpr const_iterator begin() const noexcept {
		return p_;
	}
	constexpr const_iterator end() const noexcept {
		return p_ + sz_;
	}
	constexpr char operator[](std::size_t n) const {
		return n < sz_ ? p_[n] : throw std::out_of_range("static_string");
	}
};

inline std::ostream& operator<<(std::ostream& os, const static_string& s) {
	return os.write(s.data(), s.size());
}

inline std::string static_to_string(const static_string& s) {
	return std::string(s.begin(), s.end());
}

template<class T>
constexpr static_string type_name() {
#if defined(__clang__) || defined(__cppcheck__)
	static_string p{__PRETTY_FUNCTION__};
	return static_string(p.data() + 31, p.size() - 31 - 1);
#elif defined(__GNUC__)
	static_string p{__PRETTY_FUNCTION__};
#	if __cplusplus < 201402
	return static_string(p.data() + 36, p.size() - 36 - 1);
#	else
	return static_string(p.data() + 46, p.size() - 46 - 1);
#	endif
#elif defined(_MSC_VER)
	static_string p{__FUNCSIG__};
	return static_string(p.data() + 38, p.size() - 38 - 7);
#else
#	error "Unknown compiler"
#endif
}

namespace pretty_name {
template<class T>
std::string pretty_name() {
	return static_to_string(type_name<T>());
}

template<typename... Args>
std::vector<std::string> pretty_names(std::tuple<>) {
	return {};
}

template<typename... Args>
std::vector<std::string> pretty_names(std::tuple<Args...>) {
	return {pretty_name<Args>()...};
}

} // namespace pretty_name
