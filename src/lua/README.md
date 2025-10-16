# Lua Interpreter Module

A modern C++20 library providing a sandboxed Lua 5.4 runtime with Windows API bindings and filesystem operations. This library enables embedding Lua scripting capabilities into C++ applications with a focus on safety, extensibility, and ease of use.

## Features

- **Embedded Lua 5.4**: Full Lua runtime compiled from source and statically linked
- **Sol2 C++ Bindings**: Type-safe, modern C++ interface using Sol2 v3.3.0
- **Windows API Integration**: Comprehensive Win32 API bindings for system operations
- **Filesystem Module**: Cross-platform filesystem operations using C++20 std::filesystem
- **Code Extraction**: Intelligent extraction of Lua code from text/markdown
- **Error Handling**: Robust error handling with Result types and detailed error information
- **Output Capture**: Automatic capture of print statements and execution results
- **Module System**: Extensible architecture for adding new tool modules

## Architecture

```
src/lua/
├── include/                 # Public headers
│   ├── lua_api.hpp         # Main API entry point
│   ├── lua_runtime.hpp     # Core runtime with Result types
│   ├── lua_modules.hpp     # Module system with Win32/FS modules
│   ├── lua_extract.hpp     # Code extraction utilities
│   ├── lua_tools.hpp       # Tool registry for extensibility
│   ├── win32_tools.hpp     # Win32 tool bindings
│   └── filesystem_tools.hpp # Filesystem tool bindings
└── src/                     # Implementation
    ├── lua_runtime.cpp      # Runtime implementation
    ├── lua_modules.cpp      # Built-in modules
    ├── win32_tools.cpp      # Win32 implementation
    └── filesystem_tools.cpp # Filesystem implementation
```

## Quick Start

```cpp
#include "lua_api.hpp"

// Create runtime and execute code
lua::LuaRuntime runtime;
auto result = runtime.execute("print('Hello from Lua!')");

if (result.isOk()) {
    auto& output = result.value();
    std::cout << "Output: " << output.printed_output << std::endl;
    std::cout << "Time: " << output.execution_time.count() << "ns" << std::endl;
} else {
    // Handle error
    auto& error = result.error();
    std::visit([](auto&& e) {
        std::cerr << e.what() << std::endl;
    }, error);
}
```

## Adding New Tools

The library provides an extensible tool system for adding new Lua bindings. Here's how to add custom tools:

### Step 1: Create a Tool Module Header

Create a new header file in `include/` for your module:

```cpp
// include/my_tools.hpp
#ifndef MY_TOOLS_HPP
#define MY_TOOLS_HPP

#include <sol/sol.hpp>
#include "lua_tools.hpp"

namespace lua::mytools {

// Register all tools with the registry
void registerTools(lua::tools::ToolRegistry& registry, sol::state& state);

} // namespace lua::mytools

#endif // MY_TOOLS_HPP
```

### Step 2: Implement the Tool Module

Create the implementation in `src/`:

```cpp
// src/my_tools.cpp
#include "my_tools.hpp"
#include <cmath>

namespace lua::mytools {

void registerTools(lua::tools::ToolRegistry& registry, sol::state& state) {
    // Example: Add a math function
    registry.bind(state, "mytools", "distance",
        "Calculate distance between two 2D points",
        [](double x1, double y1, double x2, double y2) -> double {
            double dx = x2 - x1;
            double dy = y2 - y1;
            return std::sqrt(dx * dx + dy * dy);
        }
    );
    
    // Example: Add a string utility
    registry.bind(state, "mytools", "reverse",
        "Reverse a string",
        [](std::string_view str) -> std::string {
            return std::string(str.rbegin(), str.rend());
        }
    );
    
    // Example: Add a table/object returning function
    registry.bind(state, "mytools", "getSystemInfo",
        "Get system information as a table",
        [](sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            sol::table info = lua.create_table();
            info["version"] = "1.0.0";
            info["platform"] = "Windows";
            info["cores"] = std::thread::hardware_concurrency();
            return info;
        }
    );
}

} // namespace lua::mytools
```

### Step 3: Create a Module Class (Alternative Approach)

For more complex modules, use the module class pattern:

```cpp
// include/custom_module.hpp
#ifndef CUSTOM_MODULE_HPP
#define CUSTOM_MODULE_HPP

#include "lua_modules.hpp"

namespace lua::modules {

class CustomModule {
public:
    static void install(sol::state& state);
    static constexpr std::string_view name() { return "custom"; }
    
private:
    // Helper methods
    static void installMathFunctions(sol::table& module);
    static void installIOFunctions(sol::table& module);
    static void installConstants(sol::table& module);
};

} // namespace lua::modules

#endif // CUSTOM_MODULE_HPP
```

Implementation:

```cpp
// src/custom_module.cpp
#include "custom_module.hpp"

namespace lua::modules {

void CustomModule::install(sol::state& state) {
    auto custom = state["custom"].get_or_create<sol::table>();
    
    installMathFunctions(custom);
    installIOFunctions(custom);
    installConstants(custom);
}

void CustomModule::installMathFunctions(sol::table& module) {
    // Bind a simple function
    module["add"] = [](double a, double b) { return a + b; };
    
    // Bind a function with multiple return values
    module["divmod"] = [](int a, int b) -> std::tuple<int, int> {
        return {a / b, a % b};
    };
    
    // Bind a function that accepts tables
    module["sum"] = [](sol::table numbers) -> double {
        double total = 0;
        for (auto& pair : numbers) {
            if (pair.second.is<double>()) {
                total += pair.second.as<double>();
            }
        }
        return total;
    };
}

void CustomModule::installIOFunctions(sol::table& module) {
    // Function with optional parameters
    module["log"] = [](std::string_view message, 
                       sol::optional<std::string_view> level) {
        std::string log_level = level.value_or("INFO");
        std::cout << "[" << log_level << "] " << message << std::endl;
    };
    
    // Function that throws errors
    module["requireFile"] = [](std::string_view path) -> std::string {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("File not found: " + std::string(path));
        }
        // Read file content...
        return "file content";
    };
}

void CustomModule::installConstants(sol::table& module) {
    module["VERSION"] = "1.0.0";
    module["MAX_BUFFER_SIZE"] = 4096;
    module["PI"] = 3.14159265359;
}

} // namespace lua::modules
```

### Step 4: Register the Module

Register your module with the runtime:

```cpp
#include "lua_api.hpp"
#include "custom_module.hpp"
#include "my_tools.hpp"

// Using the module registry
lua::LuaRuntime runtime;
lua::modules::ModuleRegistry registry(runtime.getState());

// Register standard modules
registry.registerStandardModules();

// Register custom module
registry.registerModule<lua::modules::CustomModule>();

// Or register tools directly
lua::tools::ToolRegistry tool_registry;
lua::mytools::registerTools(tool_registry, runtime.getState());

// Now you can use the modules in Lua
runtime.execute(R"(
    -- Using custom module
    local result = custom.add(10, 20)
    print("Sum: " .. result)
    
    local q, r = custom.divmod(17, 5)
    print("Quotient: " .. q .. ", Remainder: " .. r)
    
    -- Using mytools
    local dist = mytools.distance(0, 0, 3, 4)
    print("Distance: " .. dist)  -- Outputs: 5
)");
```

## Implementation Guide for Developers

### Core Components

#### 1. Result Type Pattern
The library uses a Result<T> type for error handling, inspired by Rust:

```cpp
template<typename T>
class Result {
    std::variant<T, Error> data;
public:
    bool isOk() const;
    bool isError() const;
    const T& value() const;
    const Error& error() const;
    
    // Functional transformations
    template<typename Fn>
    auto map(Fn&& fn) -> Result<decltype(fn(std::declval<T>()))>;
};
```

#### 2. Error Types
Three error types provide detailed error information:

```cpp
struct SyntaxError {
    std::string message;
    int line;
};

struct RuntimeError {
    std::string message;
    std::optional<int> line;
    std::string stack_trace;
};

using Error = std::variant<SyntaxError, RuntimeError>;
```

#### 3. Module System Architecture
Modules follow a concept-based design:

```cpp
template<typename T>
concept LuaModule = requires(T t, sol::state& state) {
    { T::install(state) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<std::string_view>;
};
```

#### 4. Tool Registry Pattern
The ToolRegistry manages function documentation and bindings:

```cpp
class ToolRegistry {
    std::vector<std::string> documentation;
    std::map<std::string, sol::object> functions;
public:
    template<typename Func>
    void bind(sol::state& state, 
              const std::string& module,
              const std::string& name,
              const std::string& doc,
              Func&& func);
              
    std::string getSystemPrompt() const;  // Generate AI prompts
};
```

### Best Practices

#### Type Safety
Use Sol2's type checking for safe conversions:

```cpp
module["safeDivide"] = [](double a, double b) -> sol::optional<double> {
    if (b == 0) return sol::nullopt;
    return a / b;
};
```

#### Memory Management
Sol2 handles Lua/C++ memory boundaries automatically:

```cpp
// Safe: Sol2 manages lifetime
module["createBuffer"] = []() -> std::vector<uint8_t> {
    return std::vector<uint8_t>(1024, 0);
};
```

#### Error Handling
Throw exceptions for errors - Sol2 converts them to Lua errors:

```cpp
module["validateInput"] = [](int value) {
    if (value < 0) {
        throw std::invalid_argument("Value must be positive");
    }
    return value * 2;
};
```

#### Thread Safety
The LuaRuntime is not thread-safe. Use one runtime per thread:

```cpp
// Each thread gets its own runtime
thread_local lua::LuaRuntime runtime;
```

### Advanced Features

#### Custom Types
Register C++ classes with Lua:

```cpp
struct Point {
    double x, y;
    double distance() const { return std::sqrt(x*x + y*y); }
};

void registerPoint(sol::state& state) {
    state.new_usertype<Point>("Point",
        sol::constructors<Point(), Point(double, double)>(),
        "x", &Point::x,
        "y", &Point::y,
        "distance", &Point::distance
    );
}
```

#### Coroutines
Support for Lua coroutines:

```cpp
runtime.execute(R"(
    co = coroutine.create(function(start)
        for i = start, start + 4 do
            coroutine.yield(i * i)
        end
    end)
    
    for i = 1, 5 do
        local ok, value = coroutine.resume(co, 10)
        print(value)
    end
)");
```

#### Sandboxing
Restrict available functions for security:

```cpp
lua::LuaRuntime sandbox;
// Don't open io library for sandboxed environment
sandbox.openLibrary(sol::lib::base);
sandbox.openLibrary(sol::lib::math);
sandbox.openLibrary(sol::lib::string);
sandbox.openLibrary(sol::lib::table);
// io, os, debug libraries not loaded
```

## Testing

Test your custom modules:

```cpp
#include <gtest/gtest.h>
#include "lua_api.hpp"
#include "custom_module.hpp"

TEST(CustomModuleTest, MathFunctions) {
    lua::LuaRuntime runtime;
    lua::modules::CustomModule::install(runtime.getState());
    
    auto result = runtime.execute(R"(
        local sum = custom.add(10, 20)
        return sum
    )");
    
    ASSERT_TRUE(result.isOk());
    auto& output = result.value();
    ASSERT_TRUE(output.return_value.has_value());
    EXPECT_EQ(output.return_value->as<double>(), 30.0);
}
```

## Dependencies

- **C++20 Compiler**: MSVC 2022, GCC 11+, or Clang 14+
- **CMake 3.20+**: Build system
- **Lua 5.4.8**: Embedded and built from source
- **Sol2 v3.3.0**: Downloaded automatically via FetchContent
- **Windows SDK**: For Win32 API bindings (Windows only)

## Platform Support

- **Windows**: Full support with Win32 API bindings
- **Linux**: Core Lua and filesystem modules
- **macOS**: Core Lua and filesystem modules

## License

This module is part of the local_ware project. See the main project LICENSE file for details.