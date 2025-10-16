#pragma once

#include <vector>
#include <string>
#include <limits>

class TestDataGenerators {
public:
    static std::vector<std::string> GetMaliciousFilePaths() {
        return {
            "../../../etc/passwd",
            "..\\..\\..\\windows\\system32\\config\\sam",
            "C:\\Windows\\System32\\drivers\\etc\\hosts",
            "/etc/shadow",
            std::string(260, 'A'),  // MAX_PATH overflow
            std::string(32768, 'X')  // Very long path
        };
    }

    static std::vector<std::string> GetMalformedUTF8Sequences() {
        return {
            "\x80",                          // Invalid start byte
            "\xC0\x80",                      // Overlong encoding
            "\xE0\x80\x80",                  // Overlong encoding
            "\xF0\x80\x80\x80",              // Overlong encoding
            "\xC2",                          // Incomplete sequence
            "\xE0\xA0",                      // Incomplete sequence
            "\xF0\x90\x80",                  // Incomplete sequence
            "\xED\xA0\x80",                  // Surrogate half
            "\xFF\xFE",                      // BOM in wrong endianness
            "Valid\xFFInvalid",              // Mixed valid/invalid
            "Test\xC0\xAFString"             // Overlong slash
        };
    }

    static std::vector<std::string> GetFormatStringAttacks() {
        return {
            "%s%s%s%s%s%s%s%s%s%s",
            "%x%x%x%x%x%x%x%x",
            "%n%n%n%n",
            "%p%p%p%p",
            "%.99999s",
            std::string(100, '%') + "n"
        };
    }

    static std::vector<std::wstring> GetConsoleInjectionAttacks() {
        return {
            L"\x1b[31mRED\x1b[0m",                    // ANSI color codes
            L"\x1b[2J\x1b[H",                         // Clear screen
            L"\r\ninjected command",                   // Line injection
            L"test\roverwrite",                       // Carriage return attack
            L"normal\x08\x08\x08hidden",              // Backspace hiding
            L"\x07\x07\x07"                           // Bell characters
        };
    }

    static std::vector<std::string> GetJSONInjectionPayloads() {
        return {
            R"({"__proto__": {"isAdmin": true}})",
            R"({"constructor": {"prototype": {"isAdmin": true}}})",
            R"({"test": "value\"});alert(1);//"})",
            "\"" + std::string(100000, 'A') + "\"",
            R"({"a":"b","a":"c"})",  // Duplicate keys
            R"({"a": 1e308})",  // Large number
            R"({"a": -1e308})"  // Large negative number
        };
    }

    static std::vector<int64_t> GetIntegerOverflowValues() {
        return {
            INT64_MIN,
            INT64_MIN + 1,
            -1,
            0,
            1,
            INT64_MAX - 1,
            INT64_MAX
        };
    }

    static std::string GenerateLargeString(size_t length, char fill = 'A') {
        return std::string(length, fill);
    }
};
