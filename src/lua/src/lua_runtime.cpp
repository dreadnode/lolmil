#include "lua_runtime.hpp"
#include <regex>
#include <iomanip>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace lua {

LuaRuntime::LuaRuntime() {
    initialize();
}

void LuaRuntime::initialize() {
    openStandardLibraries();
    setupPrintCapture();
}

void LuaRuntime::openStandardLibraries() {
    state.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::os,
        sol::lib::math,
        sol::lib::table,
        sol::lib::io,
        sol::lib::debug
    );
}

void LuaRuntime::openLibrary(sol::lib library) {
    state.open_libraries(library);
}

void LuaRuntime::setupPrintCapture() {
    state["print"] = [this](sol::variadic_args args) {
        bool first = true;
        for (auto arg : args) {
            if (!first) {
                output_capture << "\t";
            }
            first = false;
            
            sol::object obj = arg;
            if (obj.is<std::string>()) {
                output_capture << obj.as<std::string>();
            } else if (obj.is<double>()) {
                double num = obj.as<double>();
                // Format integers without decimal point
                if (num == std::floor(num) && std::abs(num) < 1e10) {
                    output_capture << static_cast<long long>(num);
                } else {
                    output_capture << std::setprecision(14) << num;
                }
            } else if (obj.is<bool>()) {
                output_capture << (obj.as<bool>() ? "true" : "false");
            } else if (obj.is<sol::nil_t>()) {
                output_capture << "nil";
            } else {
                sol::protected_function tostring = state["tostring"];
                sol::protected_function_result result = tostring(obj);
                if (result.valid()) {
                    output_capture << result.get<std::string>();
                } else {
                    output_capture << "[unprintable]";
                }
            }
        }
        output_capture << "\n";
    };
}

Result<void> LuaRuntime::checkSyntax(std::string_view code) {
    try {
        // luaL_loadstring compiles without executing
        lua_State* L = state.lua_state();
        int result = luaL_loadstring(L, code.data());

        if (result != LUA_OK) {
            std::string error_msg = lua_tostring(L, -1);
            lua_pop(L, 1);
            return parseError(error_msg);
        }

        lua_pop(L, 1);
        return {};

    } catch (const std::exception& e) {
        return Error(RuntimeError{e.what(), std::nullopt, ""});
    }
}

Result<ExecutionOutput> LuaRuntime::execute(std::string_view code) {
    if (code.empty()) {
        return Error(RuntimeError{"No code provided", std::nullopt, ""});
    }

    clearOutput();
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        sol::protected_function_result result =
            state.safe_script(std::string(code), sol::script_pass_on_error);
        
        auto end_time = std::chrono::steady_clock::now();
        auto execution_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time
        );
        
        if (!result.valid()) {
            sol::error err = result;
            return parseError(err.what());
        }
        
        ExecutionOutput output;
        output.printed_output = getCapturedOutput();
        output.execution_time = execution_time;
        
        if (result.return_count() > 0) {
            output.return_value = result.get<sol::object>(0);
        }
        
        return output;
        
    } catch (const std::exception& e) {
        return Error(RuntimeError{e.what(), std::nullopt, ""});
    }
}

Error LuaRuntime::parseError(const std::string& error_msg) const {
    // Lua error format: [string "..."]:line: message
    std::regex line_regex(R"(\[string[^\]]*\]:(\d+):\s*(.*))");
    std::smatch match;
    
    if (std::regex_search(error_msg, match, line_regex)) {
        int line = std::stoi(match[1].str());
        std::string message = match[2].str();

        if (message.find("syntax error") != std::string::npos ||
            message.find("unexpected") != std::string::npos ||
            message.find("expected") != std::string::npos) {
            return SyntaxError{message, line};
        } else {
            return RuntimeError{message, line, ""};
        }
    }

    if (error_msg.find("syntax error") != std::string::npos) {
        return SyntaxError{error_msg, 0};
    }

    return RuntimeError{error_msg, std::nullopt, ""};
}

void LuaRuntime::reset() {
    state = sol::state();
    output_capture.str("");
    output_capture.clear();
    initialize();
}

bool LuaRuntime::isValid() const noexcept {
    return state.lua_state() != nullptr;
}

} // namespace lua