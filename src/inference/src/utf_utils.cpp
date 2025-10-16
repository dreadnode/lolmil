#include "utf_utils.hpp"
#include <Windows.h>
#include <vector>

namespace utf {
    
std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    
    // Calculate required buffer size
    int size_needed = MultiByteToWideChar(
        CP_UTF8, 
        MB_ERR_INVALID_CHARS,  // Fail on invalid UTF-8
        utf8.data(), 
        static_cast<int>(utf8.size()),
        nullptr, 
        0
    );
    
    if (size_needed == 0) {
        // Invalid UTF-8 input
        return {};
    }
    
    // Perform conversion
    std::wstring result(size_needed, L'\0');
    int converted = MultiByteToWideChar(
        CP_UTF8, 
        MB_ERR_INVALID_CHARS,
        utf8.data(), 
        static_cast<int>(utf8.size()),
        result.data(), 
        size_needed
    );
    
    if (converted == 0) {
        return {};
    }
    
    return result;
}

std::string to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    
    // Calculate required buffer size
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 
        WC_ERR_INVALID_CHARS,  // Fail on invalid UTF-16
        wide.data(), 
        static_cast<int>(wide.size()),
        nullptr, 
        0, 
        nullptr, 
        nullptr
    );
    
    if (size_needed == 0) {
        // Invalid UTF-16 input
        return {};
    }
    
    // Perform conversion
    std::string result(size_needed, '\0');
    int converted = WideCharToMultiByte(
        CP_UTF8, 
        WC_ERR_INVALID_CHARS,
        wide.data(), 
        static_cast<int>(wide.size()),
        result.data(), 
        size_needed, 
        nullptr, 
        nullptr
    );
    
    if (converted == 0) {
        return {};
    }
    
    return result;
}

bool is_valid_utf8(std::string_view str) {
    if (str.empty()) {
        return true;
    }
    
    // Try to convert to wide and back - if it succeeds, it's valid UTF-8
    int wide_len = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        str.data(),
        static_cast<int>(str.size()),
        nullptr,
        0
    );
    
    return wide_len > 0;
}

} // namespace utf