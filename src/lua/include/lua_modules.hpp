#ifndef LUA_MODULES_HPP
#define LUA_MODULES_HPP

#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <filesystem>
#include <span>

namespace lua::modules {

// Base module interface using concepts
template<typename T>
concept LuaModule = requires(T t, sol::state& state) {
    { T::install(state) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<std::string_view>;
};

// Win32 module for Windows API bindings
class Win32Module {
public:
    static void install(sol::state& state);
    static constexpr std::string_view name() { return "win32"; }
    
private:
    // UTF conversion utilities (inline for header-only)
    [[nodiscard]] static std::wstring toWide(std::string_view utf8);
    [[nodiscard]] static std::string fromWide(std::wstring_view wide);
    
    // Module sub-components
    static void installMessageBox(sol::table& win32);
    static void installSystemInfo(sol::table& win32);
    static void installFileSystem(sol::table& win32);
    static void installEnvironment(sol::table& win32);
    static void installProcess(sol::table& win32);
};

// File system module using std::filesystem
class FileSystemModule {
public:
    static void install(sol::state& state);
    static constexpr std::string_view name() { return "fs"; }
    
private:
    static void installPathOperations(sol::table& fs);
    static void installFileOperations(sol::table& fs);
    static void installDirectoryOperations(sol::table& fs);
};

// Module registry for managing all modules
class ModuleRegistry {
public:
    ModuleRegistry(sol::state& state) : lua_state(state) {}
    
    // Register a module
    template<LuaModule Module>
    void registerModule() {
        Module::install(lua_state);
        registered_modules.emplace_back(Module::name());
    }
    
    // Register all standard modules
    void registerStandardModules() {
        registerModule<Win32Module>();
        registerModule<FileSystemModule>();
    }
    
    // Check if module is registered
    [[nodiscard]] bool isRegistered(std::string_view module_name) const {
        return std::find(registered_modules.begin(), 
                        registered_modules.end(), 
                        module_name) != registered_modules.end();
    }
    
    // Get list of registered modules
    [[nodiscard]] std::span<const std::string> getRegisteredModules() const {
        return registered_modules;
    }
    
private:
    sol::state& lua_state;
    std::vector<std::string> registered_modules;
};

} // namespace lua::modules

#endif // LUA_MODULES_HPP