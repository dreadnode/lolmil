#include "console_output_safe.hpp"
#include <regex>
#include <algorithm>

namespace console {

namespace {
    // Global mutex for console output synchronization
    std::mutex g_console_mutex;
    
    // Sanitize string to prevent ANSI escape code injection
    std::string sanitize(std::string_view message) {
        std::string result;
        result.reserve(message.size());
        
        for (char c : message) {
            // Remove ANSI escape sequences and control characters
            if ((c >= 32 && c != 127) || c == '\n' || c == '\t') {
                // Allow printable characters, newline, and tab
                result += c;
            } else if (c == '\r') {
                // Skip carriage return to prevent line overwriting
                continue;
            } else {
                // Replace control characters with a placeholder
                result += '?';
            }
        }
        
        // Remove any ANSI escape sequences that might have slipped through
        static const std::regex ansi_regex("\x1b\\[[0-9;]*[a-zA-Z]");
        result = std::regex_replace(result, ansi_regex, "");
        
        // Remove terminal title sequences
        static const std::regex title_regex("\x1b\\][0-9];.*?\x07");
        result = std::regex_replace(result, title_regex, "");
        
        return result;
    }
    
    std::wstring sanitize(std::wstring_view message) {
        std::wstring result;
        result.reserve(message.size());
        
        for (wchar_t c : message) {
            // Remove control characters and ANSI escape sequences
            if ((c >= 32 && c != 127) || c == L'\n' || c == L'\t') {
                // Allow printable characters, newline, and tab
                result += c;
            } else if (c == L'\r') {
                // Skip carriage return to prevent line overwriting
                continue;
            } else {
                // Replace control characters with a placeholder
                result += L'?';
            }
        }
        
        // Remove any ANSI escape sequences
        static const std::wregex ansi_regex(L"\x1b\\[[0-9;]*[a-zA-Z]");
        result = std::regex_replace(result, ansi_regex, L"");
        
        // Remove terminal title sequences
        static const std::wregex title_regex(L"\x1b\\][0-9];.*?\x07");
        result = std::regex_replace(result, title_regex, L"");
        
        return result;
    }
}

void print(std::string_view message, bool newline) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::string safe_message = sanitize(message);
    std::cout << safe_message;
    if (newline) {
        std::cout << '\n';
    }
    std::cout.flush();
}

void print(std::wstring_view message, bool newline) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::wstring safe_message = sanitize(message);
    std::wcout << safe_message;
    if (newline) {
        std::wcout << L'\n';
    }
    std::wcout.flush();
}

void error(std::string_view message) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::string safe_message = sanitize(message);
    std::cerr << safe_message << '\n';
    std::cerr.flush();
}

void error(std::wstring_view message) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::wstring safe_message = sanitize(message);
    std::wcerr << safe_message << L'\n';
    std::wcerr.flush();
}

void print_token(std::string_view token) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::string safe_token = sanitize(token);
    std::cout << safe_token;
    std::cout.flush();
}

std::string get_input() {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

} // namespace console