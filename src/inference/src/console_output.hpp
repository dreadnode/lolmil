#ifndef CONSOLE_OUTPUT_HPP
#define CONSOLE_OUTPUT_HPP
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <regex>

class ConsoleOutput {
private:
    // Sanitize string to prevent ANSI escape code injection
    static std::string sanitize(const std::string& message) {
        std::string result;
        result.reserve(message.size());
        
        for (char c : message) {
            if ((c >= 32 && c != 127) || c == '\n' || c == '\t') {
                // Allow printable ASCII, newline, and tab
                result += c;
            } else if (c == '\r') {
                continue;  // Prevent line overwriting
            } else {
                result += '?';  // Replace control chars
            }
        }

        std::regex ansi_regex("\x1b\\[[0-9;]*[a-zA-Z]");  // ANSI escape sequences
        result = std::regex_replace(result, ansi_regex, "");

        std::regex title_regex("\x1b\\][0-9];.*?\x07");  // Terminal title sequences
        result = std::regex_replace(result, title_regex, "");
        
        return result;
    }
    
    static std::wstring sanitize(const std::wstring& message) {
        std::wstring result;
        result.reserve(message.size());
        
        for (wchar_t c : message) {
            if ((c >= 32 && c != 127) || c == L'\n' || c == L'\t') {
                // Allow printable ASCII, newline, and tab
                result += c;
            } else if (c == L'\r') {
                continue;  // Prevent line overwriting
            } else {
                result += L'?';  // Replace control chars
            }
        }

        std::wregex ansi_regex(L"\x1b\\[[0-9;]*[a-zA-Z]");  // ANSI escape sequences
        result = std::regex_replace(result, ansi_regex, L"");

        std::wregex title_regex(L"\x1b\\][0-9];.*?\x07");  // Terminal title sequences
        result = std::regex_replace(result, title_regex, L"");
        
        return result;
    }
    
public:
    static void print(const std::string& message, bool newline = true) {
        std::string safe_message = sanitize(message);
        std::cout << safe_message;
        if (newline) {
          std::cout << '\n';
        }
        std::cout.flush();
    }
    
    static void print(const std::wstring& message, bool newline = true) {
        std::wstring safe_message = sanitize(message);
        std::wcout << safe_message;
        if (newline) {
          std::wcout << L'\n';
        }
        std::wcout.flush();
    }
    
    static void print(const char* message, bool newline = true) {
        print(std::string(message), newline);
    }
    
    static void print(const wchar_t* message, bool newline = true) {
        print(std::wstring(message), newline);
    }
    
    static void error(const std::string& message) {
        std::string safe_message = sanitize(message);
        std::cerr << safe_message << '\n';
        std::cerr.flush();
    }
    
    static void error(const std::wstring& message) {
        std::wstring safe_message = sanitize(message);
        std::wcerr << safe_message << L'\n';
        std::wcerr.flush();
    }
    
    static void printToken(const std::string& token) {
        std::string safe_token = sanitize(token);
        std::cout << safe_token;
        std::cout.flush();
    }
    
    static std::string getInput() {
        std::string input;
        std::getline(std::cin, input);
        return input;
    }
    
    template<typename T>
    static void debug(const std::string& label, const T& value) {
        #ifdef DEBUG
        std::string safe_label = sanitize(label);
        std::cerr << "[DEBUG] " << safe_label << ": " << value << '\n';
        #endif
    }
};

#endif // CONSOLE_OUTPUT_HPP