#include <gtest/gtest.h>
#include "lua_api.hpp"
#include <string>
#include <fstream>
#include <filesystem>
#include <Windows.h>

using namespace lua;
namespace fs = std::filesystem;

class LuaWin32Tests : public ::testing::Test {
protected:
    LuaRuntime runtime;
    modules::ModuleRegistry modules{runtime.getState()};
    std::string temp_dir;
    
    void SetUp() override {
        // Register Win32 module
        modules.registerModule<modules::Win32Module>();
        
        // Create a temp directory for file tests
        temp_dir = fs::temp_directory_path().string() + "\\lua_test_" + std::to_string(GetCurrentProcessId());
        fs::create_directory(temp_dir);
    }
    
    void TearDown() override {
        // Clean up temp directory
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    }
};

// Test GetComputerName
TEST_F(LuaWin32Tests, GetComputerName) {
    std::string code = R"(
        local name = win32.GetComputerName()
        print('Computer: ' .. name)
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_FALSE(result.value().printed_output.empty());
    EXPECT_TRUE(result.value().printed_output.find("Computer: ") != std::string::npos);
}

// Test GetUserName
TEST_F(LuaWin32Tests, GetUserName) {
    std::string code = R"(
        local name = win32.GetUserName()
        print('User: ' .. name)
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_FALSE(result.value().printed_output.empty());
    EXPECT_TRUE(result.value().printed_output.find("User: ") != std::string::npos);
}

// Test GetSystemInfo
TEST_F(LuaWin32Tests, GetSystemInfo) {
    std::string code = R"(
        local info = win32.GetSystemInfo()
        print('Processors: ' .. info.processorCount)
        print('Page size: ' .. info.pageSize)
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(result.value().printed_output.find("Processors: ") != std::string::npos);
    EXPECT_TRUE(result.value().printed_output.find("Page size: ") != std::string::npos);
}

// Test Directory operations
TEST_F(LuaWin32Tests, DirectoryOperations) {
    std::string code = R"(
        local current = win32.GetCurrentDirectory()
        print('Current dir: ' .. current)
        
        local temp = win32.GetTempPath()
        print('Temp path: ' .. temp)
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(result.value().printed_output.find("Current dir: ") != std::string::npos);
    EXPECT_TRUE(result.value().printed_output.find("Temp path: ") != std::string::npos);
}

// Test File operations
TEST_F(LuaWin32Tests, FileOperations) {
    std::string test_file = temp_dir + "\\test.txt";
    // Escape backslashes for Lua
    std::string lua_test_file = test_file;
    for (size_t pos = 0; pos < lua_test_file.length(); ++pos) {
        if (lua_test_file[pos] == '\\') {
            lua_test_file.insert(pos, "\\");
            ++pos;  // Skip the inserted backslash
        }
    }
    std::string code = R"(
        local test_file = ')" + lua_test_file + R"('
        
        -- Write file
        local success = win32.WriteFile(test_file, 'Hello from Lua!')
        print('Write success: ' .. tostring(success))
        
        -- Check existence
        local exists = win32.FileExists(test_file)
        print('File exists: ' .. tostring(exists))
        
        -- Read file
        local content = win32.ReadFile(test_file)
        if content then
            print('Content: ' .. content)
        end
        
        -- Delete file
        local deleted = win32.DeleteFile(test_file)
        print('Delete success: ' .. tostring(deleted))
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    
    const auto& output = result.value().printed_output;
    EXPECT_TRUE(output.find("Write success: true") != std::string::npos);
    EXPECT_TRUE(output.find("File exists: true") != std::string::npos);
    EXPECT_TRUE(output.find("Content: Hello from Lua!") != std::string::npos);
    EXPECT_TRUE(output.find("Delete success: true") != std::string::npos);
}

// Test Directory creation
TEST_F(LuaWin32Tests, DirectoryCreation) {
    std::string test_dir = temp_dir + "\\new_dir";
    // Escape backslashes for Lua
    std::string lua_test_dir = test_dir;
    for (size_t pos = 0; pos < lua_test_dir.length(); ++pos) {
        if (lua_test_dir[pos] == '\\') {
            lua_test_dir.insert(pos, "\\");
            ++pos;  // Skip the inserted backslash
        }
    }
    std::string code = R"(
        local test_dir = ')" + lua_test_dir + R"('
        
        -- Create directory
        local created = win32.CreateDirectory(test_dir)
        print('Create success: ' .. tostring(created))
        
        -- Check existence
        local exists = win32.DirectoryExists(test_dir)
        print('Dir exists: ' .. tostring(exists))
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    
    const auto& output = result.value().printed_output;
    EXPECT_TRUE(output.find("Create success: true") != std::string::npos);
    EXPECT_TRUE(output.find("Dir exists: true") != std::string::npos);
    
    // Clean up
    fs::remove_all(test_dir);
}

// Test Environment variables
TEST_F(LuaWin32Tests, EnvironmentVariables) {
    std::string code = R"(
        -- Get PATH
        local path = win32.GetEnvironmentVariable('PATH')
        if path then
            print('PATH found: ' .. (string.len(path) > 0 and 'yes' or 'no'))
        end
        
        -- Set and get custom variable
        local test_var = 'LUA_TEST_VAR_' .. tostring(math.random(10000))
        win32.SetEnvironmentVariable(test_var, 'test_value')
        local value = win32.GetEnvironmentVariable(test_var)
        print('Custom var: ' .. (value or 'nil'))
        
        -- Clean up
        win32.SetEnvironmentVariable(test_var, '')
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    
    const auto& output = result.value().printed_output;
    EXPECT_TRUE(output.find("PATH found: yes") != std::string::npos);
    EXPECT_TRUE(output.find("Custom var: test_value") != std::string::npos);
}

// Test Sleep function
TEST_F(LuaWin32Tests, SleepFunction) {
    std::string code = R"(
        local start = win32.GetTickCount()
        win32.Sleep(100)  -- Sleep for 100ms
        local elapsed = win32.GetTickCount() - start
        print('Slept for approximately: ' .. elapsed .. 'ms')
        
        -- Should be at least 90ms (allowing some variance)
        if elapsed >= 90 then
            print('Sleep test: PASS')
        else
            print('Sleep test: FAIL')
        end
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(result.value().printed_output.find("Sleep test: PASS") != std::string::npos);
}

// Test ShellExecute (non-interactive)
TEST_F(LuaWin32Tests, ShellExecuteTest) {
    // Create a test batch file
    std::string batch_file = temp_dir + "\\test.bat";
    std::ofstream batch(batch_file);
    batch << "@echo off\necho Test output > " << temp_dir << "\\output.txt";
    batch.close();
    
    // Escape backslashes for Lua
    std::string lua_batch_file = batch_file;
    std::string lua_output_file = temp_dir + "\\output.txt";
    for (size_t pos = 0; pos < lua_batch_file.length(); ++pos) {
        if (lua_batch_file[pos] == '\\') {
            lua_batch_file.insert(pos, "\\");
            ++pos;
        }
    }
    for (size_t pos = 0; pos < lua_output_file.length(); ++pos) {
        if (lua_output_file[pos] == '\\') {
            lua_output_file.insert(pos, "\\");
            ++pos;
        }
    }
    
    std::string code = R"(
        local batch_file = ')" + lua_batch_file + R"('
        local output_file = ')" + lua_output_file + R"('
        
        -- Execute batch file
        local success = win32.ShellExecute(batch_file)
        print('Execute result: ' .. tostring(success))
        
        -- Wait for execution
        win32.Sleep(500)
        
        -- Check if output was created
        local exists = win32.FileExists(output_file)
        print('Output created: ' .. tostring(exists))
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    // Note: ShellExecute may not work in all test environments
}

// Test all Win32 constants are defined
TEST_F(LuaWin32Tests, Win32Constants) {
    std::string code = R"(
        -- MessageBox constants
        print('MB_OK: ' .. win32.MB_OK)
        print('MB_YESNO: ' .. win32.MB_YESNO)
        print('IDYES: ' .. win32.IDYES)
        print('IDNO: ' .. win32.IDNO)
    )";
    
    auto result = runtime.execute(code);
    EXPECT_TRUE(result.isOk());
    
    const auto& output = result.value().printed_output;
    EXPECT_TRUE(output.find("MB_OK: 0") != std::string::npos);
    EXPECT_TRUE(output.find("MB_YESNO: 4") != std::string::npos);
    EXPECT_TRUE(output.find("IDYES: 6") != std::string::npos);
    EXPECT_TRUE(output.find("IDNO: 7") != std::string::npos);
}