#ifndef LUA_RUNTIME_HPP
#define LUA_RUNTIME_HPP

#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <chrono>
#include <sstream>
#include <concepts>
#include <type_traits>

namespace lua {

// Error types
struct SyntaxError {
    std::string message;
    int line;
    
    [[nodiscard]] std::string what() const {
        return "Syntax error at line " + std::to_string(line) + ": " + message;
    }
};

struct RuntimeError {
    std::string message;
    std::optional<int> line;
    std::string stack_trace;
    
    [[nodiscard]] std::string what() const {
        if (line) {
            return "Runtime error at line " + std::to_string(*line) + ": " + message;
        }
        return "Runtime error: " + message;
    }
};

using Error = std::variant<SyntaxError, RuntimeError>;

// Forward declaration for specialization
template<typename T>
class Result;

// Result type for error handling
template<typename T>
class Result {
public:
    Result(T value) : data(std::move(value)) {}
    Result(Error error) : data(std::move(error)) {}
    
    [[nodiscard]] bool isOk() const noexcept {
        return std::holds_alternative<T>(data);
    }
    
    [[nodiscard]] bool isError() const noexcept {
        return std::holds_alternative<Error>(data);
    }
    
    [[nodiscard]] explicit operator bool() const noexcept {
        return isOk();
    }
    
    [[nodiscard]] const T& value() const& {
        return std::get<T>(data);
    }
    
    [[nodiscard]] T& value() & {
        return std::get<T>(data);
    }
    
    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(data));
    }
    
    [[nodiscard]] const Error& error() const {
        return std::get<Error>(data);
    }
    
    template<typename Fn>
    [[nodiscard]] auto map(Fn&& fn) const -> Result<decltype(fn(std::declval<T>()))> {
        if (isOk()) {
            return Result<decltype(fn(std::declval<T>()))>(fn(value()));
        }
        return Result<decltype(fn(std::declval<T>()))>(error());
    }
    
private:
    std::variant<T, Error> data;
};

// Specialization for void
template<>
class Result<void> {
public:
    Result() : has_error(false) {}
    Result(Error error) : has_error(true), error_value(std::move(error)) {}

    [[nodiscard]] bool isOk() const noexcept {
        return !has_error;
    }

    [[nodiscard]] bool isError() const noexcept {
        return has_error;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return isOk();
    }

    [[nodiscard]] const Error& error() const {
        return error_value;
    }

private:
    bool has_error;
    Error error_value;
};

// Execution output
struct ExecutionOutput {
    std::string printed_output;
    std::chrono::nanoseconds execution_time;
    std::optional<sol::object> return_value;
};

// Concepts for template constraints
template<typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template<typename T>
concept LuaConvertible = requires(T t, sol::state& s) {
    { s["x"] = t };
};

// Main Lua runtime class
class LuaRuntime {
public:
    LuaRuntime();
    ~LuaRuntime() = default;
    
    // Move-only semantics
    LuaRuntime(LuaRuntime&&) = default;
    LuaRuntime& operator=(LuaRuntime&&) = default;
    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    
    // Check syntax without executing
    [[nodiscard]] Result<void> checkSyntax(std::string_view code);

    // Execute Lua code
    [[nodiscard]] Result<ExecutionOutput> execute(std::string_view code);

    // Call a Lua function
    template<typename ReturnType = void, typename... Args>
    [[nodiscard]] Result<ReturnType> call(std::string_view function_name, Args&&... args) {
        try {
            sol::protected_function func = state[std::string(function_name)];
            if (!func.valid()) {
                return Error(RuntimeError{
                    "Function '" + std::string(function_name) + "' not found",
                    std::nullopt,
                    ""
                });
            }
            
            auto result = func(std::forward<Args>(args)...);
            if (!result.valid()) {
                sol::error err = result;
                return Error(RuntimeError{err.what(), std::nullopt, ""});
            }
            
            if constexpr (std::is_void_v<ReturnType>) {
                return {};
            } else {
                return result.get<ReturnType>();
            }
        } catch (const std::exception& e) {
            return Error(RuntimeError{e.what(), std::nullopt, ""});
        }
    }
    
    // Set global variable
    template<LuaConvertible T>
    void setGlobal(std::string_view name, T&& value) {
        state[std::string(name)] = std::forward<T>(value);
    }
    
    // Get global variable
    template<typename T>
    [[nodiscard]] Result<T> getGlobal(std::string_view name) const {
        try {
            sol::object obj = state[std::string(name)];
            if (!obj.valid() || obj.is<sol::nil_t>()) {
                return Error(RuntimeError{
                    "Global '" + std::string(name) + "' not found",
                    std::nullopt,
                    ""
                });
            }
            return obj.as<T>();
        } catch (const std::exception& e) {
            return Error(RuntimeError{e.what(), std::nullopt, ""});
        }
    }
    
    // Register a module
    template<typename ModuleFn>
    requires std::invocable<ModuleFn, sol::state&>
    void registerModule(std::string_view name, ModuleFn&& module_fn) {
        auto module_table = state[std::string(name)].get_or_create<sol::table>();
        module_fn(state);
    }
    
    // Access to raw Sol state for advanced usage
    [[nodiscard]] sol::state& getState() noexcept { return state; }
    [[nodiscard]] const sol::state& getState() const noexcept { return state; }
    
    // State management
    void reset();
    [[nodiscard]] bool isValid() const noexcept;
    
    // Configure libraries
    void openStandardLibraries();
    void openLibrary(sol::lib library);
    
    // Get captured output
    [[nodiscard]] std::string getCapturedOutput() const {
        return output_capture.str();
    }
    
    void clearOutput() {
        output_capture.str("");
        output_capture.clear();
    }
    
private:
    sol::state state;
    mutable std::stringstream output_capture;
    
    void initialize();
    void setupPrintCapture();
    Error parseError(const std::string& error_msg) const;
};

} // namespace lua

#endif // LUA_RUNTIME_HPP