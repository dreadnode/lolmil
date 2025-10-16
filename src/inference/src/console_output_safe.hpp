#ifndef CONSOLE_OUTPUT_SAFE_HPP
#define CONSOLE_OUTPUT_SAFE_HPP

#include <iostream>
#include <string>
#include <string_view>
#include <mutex>
#include <sstream>

namespace console {

// Thread-safe console output functions
void print(std::string_view message, bool newline = true);
void print(std::wstring_view message, bool newline = true);

// Print with variadic arguments (simple concatenation for now)
template<typename T>
void print_args(const T& first) {
    std::stringstream ss;
    ss << first;
    print(ss.str());
}

template<typename T, typename... Args>
void print_args(const T& first, const Args&... args) {
    std::stringstream ss;
    ss << first;
    ((ss << args), ...);
    print(ss.str());
}

// Error output
void error(std::string_view message);
void error(std::wstring_view message);

// Error with variadic arguments
template<typename T, typename... Args>
void error_args(const T& first, const Args&... args) {
    std::stringstream ss;
    ss << first;
    ((ss << args), ...);
    error(ss.str());
}

// Token output (no newline by default)
void print_token(std::string_view token);

// Get user input
[[nodiscard]] std::string get_input();

// Debug output (only in debug builds)
template<typename T>
void debug(std::string_view label, const T& value) {
#ifdef DEBUG
    static std::mutex debug_mutex;
    std::lock_guard<std::mutex> lock(debug_mutex);
    std::cerr << "[DEBUG] " << label << ": " << value << '\n';
#else
    (void)label;
    (void)value;
#endif
}

} // namespace console

#endif // CONSOLE_OUTPUT_SAFE_HPP