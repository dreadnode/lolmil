#ifndef UTF_UTILS_HPP
#define UTF_UTILS_HPP

#include <string>
#include <string_view>

namespace utf {
    // Convert UTF-8 string to UTF-16 wide string (Windows)
    [[nodiscard]] std::wstring to_wide(std::string_view utf8);
    
    // Convert UTF-16 wide string to UTF-8 string
    [[nodiscard]] std::string to_utf8(std::wstring_view wide);
    
    // Validate UTF-8 string
    [[nodiscard]] bool is_valid_utf8(std::string_view str);
}

#endif // UTF_UTILS_HPP