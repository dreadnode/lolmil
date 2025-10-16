#include "lua_modules.hpp"
#include "utf_utils.hpp"
#include <Windows.h>
#include <Shellapi.h>
#include <array>
#include <vector>
#include <fstream>
#include <filesystem>

namespace lua::modules {

namespace fs = std::filesystem;

std::wstring Win32Module::toWide(std::string_view utf8) {
    return utf::to_wide(utf8);
}

std::string Win32Module::fromWide(std::wstring_view wide) {
    return utf::to_utf8(wide);
}

void Win32Module::install(sol::state& state) {
    auto win32 = state["win32"].get_or_create<sol::table>();
    
    installMessageBox(win32);
    installSystemInfo(win32);
    installFileSystem(win32);
    installEnvironment(win32);
    installProcess(win32);
}

void Win32Module::installMessageBox(sol::table& win32) {
    win32["MessageBox"] = [](std::string_view text, 
                             std::optional<std::string_view> caption, 
                             std::optional<int> type) -> int {
        auto wtext = toWide(text);
        auto wcaption = caption ? toWide(*caption) : L"Message";
        UINT uType = type.value_or(MB_OK);
        return MessageBoxW(nullptr, wtext.c_str(), wcaption.c_str(), uType);
    };

    win32["MB_OK"] = MB_OK;
    win32["MB_OKCANCEL"] = MB_OKCANCEL;
    win32["MB_YESNO"] = MB_YESNO;
    win32["MB_YESNOCANCEL"] = MB_YESNOCANCEL;
    win32["MB_ICONINFORMATION"] = MB_ICONINFORMATION;
    win32["MB_ICONWARNING"] = MB_ICONWARNING;
    win32["MB_ICONERROR"] = MB_ICONERROR;
    win32["MB_ICONQUESTION"] = MB_ICONQUESTION;

    win32["IDOK"] = IDOK;
    win32["IDCANCEL"] = IDCANCEL;
    win32["IDYES"] = IDYES;
    win32["IDNO"] = IDNO;
}

void Win32Module::installSystemInfo(sol::table& win32) {
    win32["GetSystemInfo"] = [](sol::this_state s) -> sol::table {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        
        sol::state_view lua(s);
        sol::table info = lua.create_table();
        info["processorCount"] = si.dwNumberOfProcessors;
        info["pageSize"] = si.dwPageSize;
        info["processorArchitecture"] = si.wProcessorArchitecture;
        info["processorLevel"] = si.wProcessorLevel;
        info["processorRevision"] = si.wProcessorRevision;
        return info;
    };

    win32["GetComputerName"] = []() -> std::string {
        std::array<wchar_t, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(buffer.data(), &size)) {
            return fromWide(buffer.data());
        }
        return "";
    };
    
    win32["GetUserName"] = []() -> std::string {
        std::array<wchar_t, 256> buffer{};
        DWORD size = 256;
        if (GetUserNameW(buffer.data(), &size)) {
            return fromWide(buffer.data());
        }
        return "";
    };

    win32["Sleep"] = [](int milliseconds) {
        Sleep(static_cast<DWORD>(milliseconds));
    };
    
    win32["GetTickCount"] = []() -> unsigned int {
        return GetTickCount();
    };
}

void Win32Module::installFileSystem(sol::table& win32) {
    win32["GetCurrentDirectory"] = []() -> std::string {
        DWORD size = GetCurrentDirectoryW(0, nullptr);
        if (size == 0) return "";
        
        std::vector<wchar_t> buffer(size);
        if (GetCurrentDirectoryW(size, buffer.data())) {
            return fromWide(buffer.data());
        }
        return "";
    };
    
    win32["SetCurrentDirectory"] = [](std::string_view path) -> bool {
        return SetCurrentDirectoryW(toWide(path).c_str()) != 0;
    };
    
    win32["GetTempPath"] = []() -> std::string {
        std::array<wchar_t, MAX_PATH> buffer{};
        DWORD size = GetTempPathW(MAX_PATH, buffer.data());
        if (size > 0 && size < MAX_PATH) {
            return fromWide(buffer.data());
        }
        return "";
    };

    win32["WriteFile"] = [](std::string_view filename, std::string_view content) -> bool {
        try {
            std::ofstream file(std::string(filename), std::ios::binary);
            if (file) {
                file.write(content.data(), content.size());
                return file.good();
            }
        } catch (...) {
            // Silently fail on write errors
        }
        return false;
    };
    
    win32["ReadFile"] = [](std::string_view filename) -> sol::optional<std::string> {
        try {
            std::ifstream file(std::string(filename), std::ios::binary | std::ios::ate);
            if (file) {
                auto size = file.tellg();
                file.seekg(0);
                std::string buffer(size, '\0');
                if (file.read(buffer.data(), size)) {
                    return buffer;
                }
            }
        } catch (...) {
            // Silently fail on read errors
        }
        return sol::nullopt;
    };
    
    win32["FileExists"] = [](std::string_view filename) -> bool {
        auto wpath = toWide(filename);
        DWORD attrs = GetFileAttributesW(wpath.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES && 
                !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    };
    
    win32["DirectoryExists"] = [](std::string_view path) -> bool {
        auto wpath = toWide(path);
        DWORD attrs = GetFileAttributesW(wpath.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES && 
                (attrs & FILE_ATTRIBUTE_DIRECTORY));
    };
    
    win32["CreateDirectory"] = [](std::string_view path) -> bool {
        auto wpath = toWide(path);
        return CreateDirectoryW(wpath.c_str(), nullptr) != 0 || 
               GetLastError() == ERROR_ALREADY_EXISTS;
    };
    
    win32["DeleteFile"] = [](std::string_view filename) -> bool {
        return DeleteFileW(toWide(filename).c_str()) != 0;
    };
}

void Win32Module::installEnvironment(sol::table& win32) {
    win32["GetEnvironmentVariable"] = [](std::string_view name) -> sol::optional<std::string> {
        auto wname = toWide(name);
        DWORD size = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
        if (size == 0) return sol::nullopt;
        
        std::vector<wchar_t> buffer(size);
        if (GetEnvironmentVariableW(wname.c_str(), buffer.data(), size)) {
            return fromWide(buffer.data());
        }
        return sol::nullopt;
    };
    
    win32["SetEnvironmentVariable"] = [](std::string_view name, 
                                         std::string_view value) -> bool {
        return SetEnvironmentVariableW(toWide(name).c_str(), 
                                       toWide(value).c_str()) != 0;
    };
}

void Win32Module::installProcess(sol::table& win32) {
    win32["ShellExecute"] = [](std::string_view file, 
                               std::optional<std::string_view> params) -> bool {
        auto wfile = toWide(file);
        auto wparams = params ? toWide(*params) : L"";
        
        HINSTANCE result = ShellExecuteW(
            nullptr, L"open", wfile.c_str(),
            wparams.empty() ? nullptr : wparams.c_str(),
            nullptr, SW_SHOW
        );
        
        return reinterpret_cast<intptr_t>(result) > 32;
    };
}

void FileSystemModule::install(sol::state& state) {
    auto fs_table = state["fs"].get_or_create<sol::table>();
    
    installPathOperations(fs_table);
    installFileOperations(fs_table);
    installDirectoryOperations(fs_table);
}

void FileSystemModule::installPathOperations(sol::table& fs_table) {
    fs_table["join"] = [](sol::variadic_args args) -> std::string {
        fs::path result;
        for (auto arg : args) {
            if (arg.is<std::string>()) {
                result /= arg.as<std::string>();
            }
        }
        return result.string();
    };
    
    fs_table["basename"] = [](std::string_view path) -> std::string {
        return fs::path(path).filename().string();
    };
    
    fs_table["dirname"] = [](std::string_view path) -> std::string {
        return fs::path(path).parent_path().string();
    };
    
    fs_table["extension"] = [](std::string_view path) -> std::string {
        return fs::path(path).extension().string();
    };
    
    fs_table["stem"] = [](std::string_view path) -> std::string {
        return fs::path(path).stem().string();
    };
    
    fs_table["absolute"] = [](std::string_view path) -> std::string {
        std::error_code ec;
        auto result = fs::absolute(path, ec);
        return ec ? "" : result.string();
    };
}

void FileSystemModule::installFileOperations(sol::table& fs_table) {
    fs_table["exists"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::exists(path, ec);
    };
    
    fs_table["is_file"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    };
    
    fs_table["is_directory"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::is_directory(path, ec);
    };
    
    fs_table["file_size"] = [](std::string_view path) -> sol::optional<uintmax_t> {
        std::error_code ec;
        auto size = fs::file_size(path, ec);
        return ec ? sol::nullopt : sol::optional<uintmax_t>(size);
    };
    
    fs_table["copy"] = [](std::string_view from, std::string_view to) -> bool {
        std::error_code ec;
        fs::copy(from, to, fs::copy_options::overwrite_existing, ec);
        return !ec;
    };
    
    fs_table["rename"] = [](std::string_view from, std::string_view to) -> bool {
        std::error_code ec;
        fs::rename(from, to, ec);
        return !ec;
    };
    
    fs_table["remove"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::remove(path, ec);
    };
}

void FileSystemModule::installDirectoryOperations(sol::table& fs_table) {
    fs_table["create_directory"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::create_directory(path, ec);
    };
    
    fs_table["create_directories"] = [](std::string_view path) -> bool {
        std::error_code ec;
        return fs::create_directories(path, ec);
    };
    
    fs_table["list_directory"] = [](std::string_view path, sol::this_state s) -> sol::table {
        sol::state_view lua(s);
        sol::table result = lua.create_table();
        int index = 1;
        
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (!ec) {
                result[index++] = entry.path().string();
            }
        }
        
        return result;
    };
    
    fs_table["remove_all"] = [](std::string_view path) -> sol::optional<uintmax_t> {
        std::error_code ec;
        auto count = fs::remove_all(path, ec);
        return ec ? sol::nullopt : sol::optional<uintmax_t>(count);
    };
}

} // namespace lua::modules