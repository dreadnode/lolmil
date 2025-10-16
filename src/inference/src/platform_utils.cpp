#include "platform_utils.hpp"
#include "utf_utils.hpp"
#include <mutex>

namespace platform {
    std::wstring utf8_to_wide(const std::string& str) {
        return utf::to_wide(str);
    }
    
    std::string wide_to_utf8(const std::wstring& str) {
        return utf::to_utf8(str);
    }
}  // namespace platform

namespace encoding {
    std::string convert_to_utf8(const std::wstring& wide) {
        return utf::to_utf8(wide);
    }
    
    std::wstring convert_from_utf8(const std::string& utf8) {
        return utf::to_wide(utf8);
    }
}  // namespace encoding