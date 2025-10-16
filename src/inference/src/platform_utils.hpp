#ifndef PLATFORM_UTILS_HPP
#define PLATFORM_UTILS_HPP

#include <string>

namespace platform {
    [[nodiscard]] std::wstring utf8_to_wide(const std::string& str);
    [[nodiscard]] std::string wide_to_utf8(const std::wstring& str);
}

namespace encoding {
    [[nodiscard]] std::string convert_to_utf8(const std::wstring& wide);
    [[nodiscard]] std::wstring convert_from_utf8(const std::string& utf8);
}

#endif // PLATFORM_UTILS_HPP