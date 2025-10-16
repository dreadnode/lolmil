#include <gtest/gtest.h>
#include "lua_api.hpp"
#include <string>
#include <chrono>

using namespace lua;

class LuaRuntimeTests : public ::testing::Test {
protected:
    LuaRuntime runtime;
    modules::ModuleRegistry modules{runtime.getState()};
    
    void SetUp() override {
        modules.registerStandardModules();
    }
};

// Test basic Lua execution
TEST_F(LuaRuntimeTests, ExecuteBasicScript) {
    std::string code = "print('Hello from Lua!')";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Hello from Lua!\n");
}

// Test multiple print statements
TEST_F(LuaRuntimeTests, MultiplePrintStatements) {
    std::string code = R"(
        print('Line 1')
        print('Line 2')
        print('Line 3')
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Line 1\nLine 2\nLine 3\n");
}

// Test print with multiple arguments
TEST_F(LuaRuntimeTests, PrintMultipleArguments) {
    std::string code = "print('Hello', 'World', 123, true, nil)";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Hello\tWorld\t123\ttrue\tnil\n");
}

// Test arithmetic operations
TEST_F(LuaRuntimeTests, ArithmeticOperations) {
    std::string code = R"(
        local a = 10
        local b = 20
        print(a + b)
        print(a * b)
        print(b / a)
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "30\n200\n2\n");
}

// Test string operations
TEST_F(LuaRuntimeTests, StringOperations) {
    std::string code = R"(
        local str = 'Hello' .. ' ' .. 'World'
        print(str)
        print(string.upper(str))
        print(string.len(str))
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Hello World\nHELLO WORLD\n11\n");
}

// Test functions
TEST_F(LuaRuntimeTests, LuaFunctions) {
    std::string code = R"(
        function add(a, b)
            return a + b
        end
        
        function greet(name)
            return 'Hello, ' .. name .. '!'
        end
        
        print(add(5, 3))
        print(greet('Lua'))
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "8\nHello, Lua!\n");
}

// Test tables
TEST_F(LuaRuntimeTests, LuaTables) {
    std::string code = R"(
        local person = {
            name = 'John',
            age = 30,
            city = 'New York'
        }
        
        print(person.name)
        print(person.age)
        
        local numbers = {10, 20, 30, 40}
        print(numbers[1])
        print(numbers[4])
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "John\n30\n10\n40\n");
}

// Test loops
TEST_F(LuaRuntimeTests, Loops) {
    std::string code = R"(
        -- For loop
        for i = 1, 3 do
            print('Count: ' .. i)
        end
        
        -- While loop
        local j = 0
        while j < 2 do
            print('While: ' .. j)
            j = j + 1
        end
    )";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Count: 1\nCount: 2\nCount: 3\nWhile: 0\nWhile: 1\n");
}

// Test error handling with modern API
TEST_F(LuaRuntimeTests, SyntaxError) {
    std::string code = "this is not valid lua code!!!";
    auto result = runtime.execute(code);
    
    EXPECT_FALSE(result.isOk());
    // Check that we get an error
    const auto& error = result.error();
    std::string error_msg = std::visit([](const auto& err) {
        return err.what();
    }, error);
    EXPECT_FALSE(error_msg.empty());
}

// Test runtime error
TEST_F(LuaRuntimeTests, RuntimeError) {
    std::string code = R"(
        local a = 10
        local b = nil
        print(a + b)  -- This will cause a runtime error
    )";
    auto result = runtime.execute(code);
    
    EXPECT_FALSE(result.isOk());
}

// Test reset functionality
TEST_F(LuaRuntimeTests, ResetState) {
    // First execution
    std::string code1 = "global_var = 42\nprint(global_var)";
    auto result1 = runtime.execute(code1);
    EXPECT_TRUE(result1.isOk());
    EXPECT_EQ(result1.value().printed_output, "42\n");
    
    // Reset the state
    runtime.reset();
    // Re-register modules after reset
    modules::ModuleRegistry new_modules(runtime.getState());
    new_modules.registerStandardModules();
    
    // Try to access the global variable after reset
    std::string code2 = "print(global_var)";
    auto result2 = runtime.execute(code2);
    EXPECT_TRUE(result2.isOk());
    EXPECT_EQ(result2.value().printed_output, "nil\n");
}

// Test empty script
TEST_F(LuaRuntimeTests, EmptyScript) {
    std::string code = "";
    auto result = runtime.execute(code);
    
    EXPECT_FALSE(result.isOk());
}

// Test error line tracking with modern API
TEST_F(LuaRuntimeTests, ErrorLineTracking) {
    std::string code = R"(
local x = 10
local y = 20
undefined_function()  -- This should cause an error on line 4
local z = x + y
)";
    auto result = runtime.execute(code);
    
    EXPECT_FALSE(result.isOk());
    
    // Check if we have a RuntimeError with line information
    const auto& error = result.error();
    bool has_line_info = std::visit([](const auto& err) {
        if constexpr (std::is_same_v<std::decay_t<decltype(err)>, RuntimeError>) {
            return err.line.has_value();
        }
        return false;
    }, error);
    
    EXPECT_TRUE(has_line_info);
}

// Test string_view interface
TEST_F(LuaRuntimeTests, StringViewInterface) {
    std::string_view code = "print('Hello from string_view!')";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "Hello from string_view!\n");
}

// Test execution timing
TEST_F(LuaRuntimeTests, ExecutionTiming) {
    std::string code = "for i = 1, 1000 do local x = i * 2 end print('Done')";
    auto result = runtime.execute(code);
    
    EXPECT_TRUE(result.isOk());
    // Check that we have timing information
    EXPECT_GT(result.value().execution_time.count(), 0);
}

// Test calling Lua functions from C++
TEST_F(LuaRuntimeTests, CallLuaFunction) {
    // Define a function
    std::string code = "function multiply(a, b) return a * b end";
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    
    // Call the function
    auto call_result = runtime.call<int>("multiply", 5, 7);
    EXPECT_TRUE(call_result.isOk());
    EXPECT_EQ(call_result.value(), 35);
}

// Test setting and getting globals
TEST_F(LuaRuntimeTests, GlobalVariables) {
    // Set a global from C++
    runtime.setGlobal("test_value", 123);
    
    // Access it from Lua
    std::string code = "print(test_value * 2)";
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "246\n");
    
    // Get it back from C++
    auto get_result = runtime.getGlobal<int>("test_value");
    EXPECT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value(), 123);
}