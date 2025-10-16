#include <gtest/gtest.h>
#include "lua_api.hpp"
#include <string>

using namespace lua;

// Test extraction from ```lua blocks
TEST(LuaExtractorTests, ExtractFromLuaCodeBlock) {
    std::string text = R"(
Here's a simple Lua script:

```lua
function greet(name)
    print("Hello, " .. name)
end

greet("World")
```

That's the script!
)";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("function greet") != std::string::npos);
    EXPECT_TRUE(extracted->find("greet(\"World\")") != std::string::npos);
}

// Test extraction from generic code blocks
TEST(LuaExtractorTests, ExtractFromGenericCodeBlock) {
    std::string text = R"(
Here's some code:

```
local x = 10
local y = 20
print(x + y)
```
)";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("local x = 10") != std::string::npos);
    EXPECT_TRUE(extracted->find("print(x + y)") != std::string::npos);
}

// Test extraction when no code is present
TEST(LuaExtractorTests, NoCodePresent) {
    std::string text = "This is just regular text with no code whatsoever.";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_FALSE(extracted.has_value());
}

// Test extraction from text with Lua patterns
TEST(LuaExtractorTests, ExtractFromInlinePatterns) {
    std::string text = R"(Here's a Lua script:
function factorial(n)
    if n <= 1 then
        return 1
    else
        return n * factorial(n - 1)
    end
end

print(factorial(5)))";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("function factorial") != std::string::npos);
    // The extractor captures the inline code without perfect formatting
    EXPECT_TRUE(extracted->find("factorial") != std::string::npos);
}

// Test extraction with mixed language blocks
TEST(LuaExtractorTests, MixedLanguageBlocks) {
    std::string text = R"(
```python
def hello():
    print("Python")
```

```lua
function hello()
    print("Lua")
end
```
)";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("print(\"Lua\")") != std::string::npos);
    EXPECT_FALSE(extracted->find("print(\"Python\")") != std::string::npos);
}

// Test whole text as Lua code
TEST(LuaExtractorTests, WholeTextAsLuaCode) {
    std::string text = R"(local function calculate(a, b)
    return a * b + 10
end

local result = calculate(5, 3)
print("Result:", result))";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("local function calculate") != std::string::npos);
    EXPECT_TRUE(extracted->find("return a * b + 10") != std::string::npos);
}

// Test extraction with Lua-specific keywords
TEST(LuaExtractorTests, LuaKeywordDetection) {
    std::string text = R"(
function test()
    local x = 10
    if x > 5 then
        print("Greater")
    elseif x < 5 then
        print("Lesser")
    else
        print("Equal")
    end
end
)";
    
    auto extracted = extract::extractLuaCode(text);
    
    EXPECT_TRUE(extracted.has_value());
    EXPECT_TRUE(extracted->find("elseif") != std::string::npos);
}

// Test extraction of all Lua blocks
TEST(LuaExtractorTests, ExtractAllLuaBlocks) {
    std::string text = R"(
First block:
```lua
print("First")
```

Second block:
```lua
print("Second")
```

Third:
```
local x = 10
print(x)
```
)";
    
    auto all_blocks = extract::extractAllLuaCode(text);
    
    // The third block has no language tag, and only has 1 Lua indicator (print)
    // So it won't be detected as Lua (needs 2+ indicators)
    EXPECT_EQ(all_blocks.size(), 2);
    if (all_blocks.size() >= 2) {
        EXPECT_TRUE(all_blocks[0].find("First") != std::string::npos);
        EXPECT_TRUE(all_blocks[1].find("Second") != std::string::npos);
    }
}

// Test code analysis
TEST(LuaExtractorTests, CodeAnalysis) {
    std::string code = R"(
function process(data)
    for i = 1, #data do
        print(data[i])
    end
end

require("module")
io.open("file.txt")

global_var = 42
)";
    
    auto analysis = extract::analyzeLuaCode(code);
    
    EXPECT_TRUE(analysis.has_loops);
    EXPECT_TRUE(analysis.has_io_operations);
    EXPECT_TRUE(analysis.has_require_statements);
    EXPECT_FALSE(analysis.has_loadstring);
    EXPECT_FALSE(analysis.has_dofile);
    
    EXPECT_EQ(analysis.defined_functions.size(), 1);
    EXPECT_EQ(analysis.defined_functions[0], "process");
    
    EXPECT_FALSE(analysis.global_writes.empty());
    EXPECT_TRUE(std::find(analysis.global_writes.begin(), 
                          analysis.global_writes.end(), 
                          "global_var") != analysis.global_writes.end());
}

// Test code block extraction with language filtering
TEST(LuaExtractorTests, ExtractCodeBlocks) {
    std::string text = R"(
```lua
print("Lua code")
```

```python
print("Python code")
```

```
print("Generic code")
```
)";
    
    auto blocks = extract::extractCodeBlocks(text);
    
    EXPECT_EQ(blocks.size(), 3);
    EXPECT_EQ(blocks[0].language, "lua");
    EXPECT_EQ(blocks[1].language, "python");
    EXPECT_EQ(blocks[2].language, "");
    
    EXPECT_TRUE(blocks[0].code.find("Lua code") != std::string::npos);
    EXPECT_TRUE(blocks[1].code.find("Python code") != std::string::npos);
    EXPECT_TRUE(blocks[2].code.find("Generic code") != std::string::npos);
}

// Test complexity analysis
TEST(LuaExtractorTests, ComplexityEstimation) {
    std::string simple_code = "print('Hello')";
    std::string complex_code = R"(
function complexFunction(data)
    for i = 1, #data do
        if data[i] > 0 then
            while data[i] > 10 do
                data[i] = data[i] / 2
            end
        end
    end
end

function helper()
    return 42
end
)";
    
    auto simple_analysis = extract::analyzeLuaCode(simple_code);
    auto complex_analysis = extract::analyzeLuaCode(complex_code);
    
    EXPECT_LT(simple_analysis.estimated_complexity, complex_analysis.estimated_complexity);
    EXPECT_EQ(complex_analysis.defined_functions.size(), 2);
}

// Test edge cases
TEST(LuaExtractorTests, EdgeCases) {
    // Empty string
    auto empty = extract::extractLuaCode("");
    EXPECT_FALSE(empty.has_value());
    
    // Just whitespace
    auto whitespace = extract::extractLuaCode("   \n\t  ");
    EXPECT_FALSE(whitespace.has_value());
    
    // Malformed code block
    std::string malformed = "```lua\nprint('unclosed')";
    auto malformed_result = extract::extractLuaCode(malformed);
    EXPECT_FALSE(malformed_result.has_value());
    
    // Non-Lua code block
    std::string non_lua = "```javascript\nconsole.log('test');\n```";
    auto non_lua_result = extract::extractLuaCode(non_lua);
    EXPECT_FALSE(non_lua_result.has_value());
}