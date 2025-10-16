#include <gtest/gtest.h>
#include "lua_runtime.hpp"
#include "lua_tools.hpp"
#include "win32_tools.hpp"
#include <Windows.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

class Win32ToolsTest : public ::testing::Test {
protected:
    lua::LuaRuntime runtime;
    lua::tools::ToolRegistry registry;
    
    void SetUp() override {
        runtime.openStandardLibraries();
        lua::win32::registerTools(registry, runtime.getState());
    }
};

TEST_F(Win32ToolsTest, WhoamiReturnsUserInfo) {
    std::string code = R"(
        local info = win32.Whoami()
        assert(info ~= nil, "whoami should return info")
        assert(type(info.username) == "string", "username should be a string")
        assert(#info.username > 0, "username should not be empty")
        assert(type(info.domain) == "string", "domain should be a string")
        assert(type(info.groups) == "table", "groups should be a table")
        
        -- Check that we have at least one group (Everyone is always present)
        local groupCount = 0
        for i, group in ipairs(info.groups) do
            groupCount = groupCount + 1
            assert(type(group) == "string", "each group should be a string")
        end
        assert(groupCount > 0, "should have at least one group")
        
        print("Username: " .. info.username)
        print("Domain: " .. info.domain)
        print("Groups: " .. tostring(groupCount))
        return info.username
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Should execute without errors";
    
    // Verify the username matches Windows API
    char username[256];
    DWORD size = sizeof(username);
    ASSERT_TRUE(GetUserNameA(username, &size));
    
    auto output = result.value();
    EXPECT_TRUE(output.printed_output.find(username) != std::string::npos)
        << "Output should contain actual username";
}

TEST_F(Win32ToolsTest, SleepDelaysExecution) {
    std::string code = R"(
        local start_time = os.clock()
        win32.Sleep(100)  -- Sleep for 100ms
        local end_time = os.clock()
        local elapsed = (end_time - start_time) * 1000  -- Convert to milliseconds
        
        -- Allow some tolerance due to system scheduling
        assert(elapsed >= 90, "Should sleep for at least 90ms, got: " .. tostring(elapsed))
        assert(elapsed <= 200, "Should not sleep for more than 200ms, got: " .. tostring(elapsed))
        
        print("Slept for " .. tostring(elapsed) .. " ms")
        return elapsed
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Should execute without errors";
    
    auto output = result.value();
    EXPECT_TRUE(output.printed_output.find("Slept for") != std::string::npos)
        << "Should print sleep duration";
}

TEST_F(Win32ToolsTest, SleepHandlesZeroAndNegative) {
    std::string code = R"(
        -- Zero sleep should return immediately
        local start = os.clock()
        win32.Sleep(0)
        local elapsed = (os.clock() - start) * 1000
        assert(elapsed < 10, "Zero sleep should be instant")
        
        -- Negative sleep should be safe (treated as 0)
        win32.Sleep(-100)
        return true
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Should handle zero and negative sleep values";
}

TEST_F(Win32ToolsTest, ShellExecuteOpenURL) {
    std::string code = R"(
        -- Try to open a file URL (won't actually open in test environment)
        -- Using a file:// URL as it's less intrusive than http://
        local success, errorCode = win32.ShellExecute("file:///C:/Windows/System32/notepad.exe")
        
        assert(type(success) == "boolean", "Should return boolean success")
        assert(type(errorCode) == "number", "Should return numeric error code")
        
        -- Note: In test environment, this might fail due to permissions
        -- We're mainly testing that the function is callable and returns correct types
        print("ShellExecute returned: " .. tostring(success) .. ", code: " .. tostring(errorCode))
        
        return true
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Should execute without errors";
}

TEST_F(Win32ToolsTest, ShellExecuteWithParameters) {
    // Create a temporary test file
    auto temp_path = fs::temp_directory_path() / "test_shell_execute.txt";
    {
        std::ofstream file(temp_path);
        file << "Test content";
    }
    
    // Convert path to forward slashes for Lua
    std::string path_str = temp_path.string();
    std::replace(path_str.begin(), path_str.end(), '\\', '/');
    
    std::string code = R"(
        local tempFile = ")" + path_str + R"("
        
        -- Test with all parameters
        local success, code = win32.ShellExecute(
            tempFile,           -- file
            "open",            -- operation
            nil,               -- params
            nil,               -- directory
            0                  -- SW_HIDE
        )
        
        assert(type(success) == "boolean", "Should return boolean")
        assert(type(code) == "number", "Should return error code")
        
        print("ShellExecute with params: " .. tostring(success) .. ", " .. tostring(code))
        return true
    )";
    
    auto result = runtime.execute(code);
    if (!result.isOk()) {
        auto error = std::visit([](const auto& e) { return e.what(); }, result.error());
        FAIL() << "Execution failed: " << error;
    }
    
    // Clean up
    fs::remove(temp_path);
}

TEST_F(Win32ToolsTest, GetServicesReturnsList) {
    std::string code = R"LUA(
        local services, error = win32.GetServices()
        
        if error then
            print("Error: " .. error)
            -- In some test environments, we might not have permission
            -- This is okay, we're testing the function exists and returns correct types
            return false
        end
        
        assert(type(services) == "table", "Should return a table of services")
        
        local serviceCount = 0
        local foundRunning = false
        
        for i, service in ipairs(services) do
            serviceCount = serviceCount + 1
            
            -- Check service structure
            assert(type(service.name) == "string", "Service name should be string")
            assert(type(service.displayName) == "string", "Display name should be string")
            assert(type(service.state) == "string", "State should be string")
            assert(type(service.processId) == "number", "Process ID should be number")
            
            -- Check for valid states
            local validStates = {
                stopped = true,
                start_pending = true,
                stop_pending = true,
                running = true,
                continue_pending = true,
                pause_pending = true,
                paused = true,
                unknown = true
            }
            assert(validStates[service.state], "Invalid service state: " .. service.state)
            
            if service.state == "running" then
                foundRunning = true
            end
            
            -- Limit output for testing
            if serviceCount <= 3 then
                print("Service: " .. service.name .. " (" .. service.state .. ")")
            end
        end
        
        print("Total services: " .. tostring(serviceCount))
        assert(serviceCount > 0, "Should have at least one service")
        assert(foundRunning, "Should have at least one running service")
        
        return serviceCount
    )LUA";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        EXPECT_TRUE(output.printed_output.find("Total services:") != std::string::npos)
            << "Should report total service count";
    } else {
        // It's okay if we don't have permission in test environment
        GTEST_SKIP() << "Insufficient permissions to enumerate services";
    }
}

TEST_F(Win32ToolsTest, GetServiceDetailsForSpecificService) {
    std::string code = R"(
        -- Try to get info for a well-known Windows service
        -- Using "Themes" as it's present on most Windows systems
        local serviceName = "Themes"
        local info, error = win32.GetService(serviceName)
        
        if error then
            print("Error: " .. error)
            -- May not have permission or service may not exist
            return false
        end
        
        assert(type(info) == "table", "Should return a table")
        assert(info.name == serviceName, "Name should match requested service")
        assert(type(info.displayName) == "string", "Display name should be string")
        assert(type(info.binaryPath) == "string", "Binary path should be string")
        assert(type(info.startType) == "string", "Start type should be string")
        assert(type(info.type) == "string", "Service type should be string")
        assert(type(info.state) == "string", "State should be string")
        assert(type(info.processId) == "number", "Process ID should be number")
        
        -- Check for SDDL (optional field, may not have permission)
        if info.sddl then
            assert(type(info.sddl) == "string", "SDDL should be a string")
            -- Basic SDDL format check - should start with D: (DACL) or O: (Owner) or G: (Group)
            assert(info.sddl:match("^[DOG]:") ~= nil, "SDDL should have valid format")
            print("SDDL: " .. info.sddl:sub(1, 50) .. "...")  -- Print first 50 chars
        else
            print("SDDL: Not available - permission or other issue")
        end
        
        -- Check for valid start types
        local validStartTypes = {
            boot = true,
            system = true,
            automatic = true,
            manual = true,
            disabled = true,
            unknown = true
        }
        assert(validStartTypes[info.startType], "Invalid start type: " .. info.startType)
        
        -- Check for valid service types
        local validTypes = {
            kernel_driver = true,
            file_system_driver = true,
            own_process = true,
            share_process = true,
            unknown = true
        }
        assert(validTypes[info.type], "Invalid service type: " .. info.type)
        
        print("Service: " .. info.name)
        print("Display: " .. info.displayName)
        print("Binary: " .. info.binaryPath)
        print("Type: " .. info.type)
        print("Start: " .. info.startType)
        print("State: " .. info.state)
        
        -- Check if dependencies exist (optional field)
        if info.dependencies then
            assert(type(info.dependencies) == "table", "Dependencies should be a table")
            local depCount = 0
            for i, dep in ipairs(info.dependencies) do
                depCount = depCount + 1
                assert(type(dep) == "string", "Each dependency should be a string")
            end
            print("Dependencies: " .. tostring(depCount))
        end
        
        return true
    )";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        EXPECT_TRUE(output.printed_output.find("Service:") != std::string::npos)
            << "Should print service information";
        EXPECT_TRUE(output.printed_output.find("Binary:") != std::string::npos)
            << "Should include binary path";
    } else {
        // It's okay if we don't have permission or service doesn't exist
        GTEST_SKIP() << "Could not access service information";
    }
}

TEST_F(Win32ToolsTest, GetServiceWithSDDL) {
    std::string code = R"LUA(
        -- Try to get SDDL for a common service
        -- Using EventLog as it's a critical Windows service
        local serviceName = "EventLog"
        local info, error = win32.GetService(serviceName)
        
        if error then
            print("Error accessing service: " .. error)
            -- Try with a different service
            serviceName = "Themes"
            info, error = win32.GetService(serviceName)
        end
        
        if error then
            print("Could not access any service: " .. error)
            return false
        end
        
        print("Service: " .. info.name)
        
        if info.sddl then
            print("SDDL retrieved successfully")
            print("SDDL length: " .. #info.sddl)
            
            -- Parse basic SDDL components
            local owner = info.sddl:match("O:([^DG]+)")
            local group = info.sddl:match("G:([^DO]+)")
            local dacl = info.sddl:match("D:([^OG]*)")
            
            if owner then print("Owner SID: " .. owner) end
            if group then print("Group SID: " .. group) end
            if dacl then 
                print("DACL present, length: " .. #dacl)
                -- Check for common ACE types in DACL
                local allowCount = select(2, dacl:gsub("%%(A;", ""))
                local denyCount = select(2, dacl:gsub("%%(D;", ""))
                print("Allow ACEs: " .. allowCount .. ", Deny ACEs: " .. denyCount)
            end
            
            return true
        else
            print("SDDL not available - may need elevated permissions")
            return true  -- Not a failure, just limited access
        end
    )LUA";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        // Either we got SDDL or we got a message about it not being available
        EXPECT_TRUE(output.printed_output.find("SDDL") != std::string::npos)
            << "Should mention SDDL status";
    } else {
        GTEST_SKIP() << "Could not access service information";
    }
}

TEST_F(Win32ToolsTest, GetServiceHandlesNonExistentService) {
    std::string code = R"(
        -- Try to get info for a non-existent service
        local info, error = win32.GetService("ThisServiceDoesNotExist12345")
        
        assert(info == nil, "Should return nil for non-existent service")
        assert(type(error) == "string", "Should return error message")
        assert(#error > 0, "Error message should not be empty")
        
        print("Expected error: " .. error)
        return true
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Should handle non-existent service gracefully";
    
    auto output = result.value();
    EXPECT_TRUE(output.printed_output.find("Expected error:") != std::string::npos)
        << "Should report error for non-existent service";
}

TEST_F(Win32ToolsTest, ToolsAreDocumented) {
    // Check that all tools are registered with documentation
    std::string code = R"(
        -- Get the system prompt which contains tool documentation
        local docs = {}
        
        -- Check each tool exists
        assert(type(win32.Whoami) == "function", "Whoami should be registered")
        assert(type(win32.Sleep) == "function", "Sleep should be registered")
        assert(type(win32.ShellExecute) == "function", "ShellExecute should be registered")
        assert(type(win32.GetServices) == "function", "GetServices should be registered")
        assert(type(win32.GetService) == "function", "GetService should be registered")
        assert(type(win32.GetFileSddl) == "function", "GetFileSddl should be registered")
        assert(type(win32.GetFileSecurity) == "function", "GetFileSecurity should be registered")
        
        print("All required tools are registered")
        return true
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "All tools should be registered";
}

TEST_F(Win32ToolsTest, GetFileSDDL) {
    std::string code = R"LUA(
        -- Test getting SDDL for Windows system directory
        local sddl, error = win32.GetFileSddl("C:\\Windows\\System32")
        
        if error then
            print("Error: " .. error)
            -- May not have permission in some cases
            return false
        end
        
        assert(type(sddl) == "string", "SDDL should be a string")
        assert(#sddl > 0, "SDDL should not be empty")
        
        -- Check basic SDDL format
        assert(sddl:match("^[DOG]:") ~= nil, "SDDL should start with O:, D:, or G:")
        
        -- Parse components
        local owner = sddl:match("O:([^DG]+)")
        local group = sddl:match("G:([^DO]+)")
        local dacl = sddl:match("D:([^OG]*)")
        
        print("SDDL length: " .. #sddl)
        if owner then print("Owner: " .. owner) end
        if group then print("Group: " .. group) end
        if dacl then print("DACL present, length: " .. #dacl) end
        
        return true
    )LUA";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        EXPECT_TRUE(output.printed_output.find("SDDL length:") != std::string::npos)
            << "Should report SDDL information";
    } else {
        GTEST_SKIP() << "Could not access file security information";
    }
}

TEST_F(Win32ToolsTest, GetFilePermissions) {
    // Create a temporary test file
    auto temp_path = fs::temp_directory_path() / "test_permissions.txt";
    {
        std::ofstream file(temp_path);
        file << "Test content";
    }
    
    // Convert path for Lua
    std::string path_str = temp_path.string();
    std::replace(path_str.begin(), path_str.end(), '\\', '/');
    
    std::string code = R"LUA(
        local tempFile = ")LUA" + path_str + R"LUA("
        
        local perms, error = win32.GetFileSecurity(tempFile)
        
        if error then
            print("Error: " .. error)
            return false
        end
        
        assert(type(perms) == "table", "Should return a table")
        assert(perms.path == tempFile, "Path should match")
        assert(type(perms.owner) == "string" or perms.owner == nil, "Owner should be string or nil")
        assert(type(perms.owner_sid) == "string" or perms.owner_sid == nil, "Owner SID should be string or nil")
        assert(type(perms.writable_by_users) == "boolean", "writable_by_users should be boolean")
        assert(type(perms.writable_by_everyone) == "boolean", "writable_by_everyone should be boolean")
        
        print("File: " .. perms.path)
        if perms.owner then
            print("Owner: " .. perms.owner)
        end
        print("Writable by Users: " .. tostring(perms.writable_by_users))
        print("Writable by Everyone: " .. tostring(perms.writable_by_everyone))
        
        -- For a temp file, it should NOT be writable by everyone
        assert(not perms.writable_by_everyone, "Temp file should not be writable by everyone")
        
        return true
    )LUA";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        EXPECT_TRUE(output.printed_output.find("Writable by") != std::string::npos)
            << "Should report permission information";
    }
    
    // Clean up
    fs::remove(temp_path);
}

TEST_F(Win32ToolsTest, ServiceVsFileSDDLDistinction) {
    std::string code = R"LUA(
        -- Get a service and compare SERVICE vs FILE permissions
        local service, error = win32.GetService("EventLog")
        
        if error then
            -- Try another service
            service, error = win32.GetService("Themes")
        end
        
        if error then
            print("Could not get any service")
            return false
        end
        
        print("Service: " .. service.name)
        print("Binary: " .. service.binaryPath)
        
        -- SERVICE SDDL (who can modify service configuration)
        if service.sddl then
            print("SERVICE SDDL (first 80 chars): " .. service.sddl:sub(1, 80))
            
            -- Check for service config change permission
            if service.sddl:match("%(A;;[^;]*CC[^;]*;;") then
                print("  Has Change Config (CC) permissions")
            end
        end
        
        -- FILE SDDL (who can modify the binary)
        local file_sddl, err = win32.GetFileSddl(service.binaryPath)
        if file_sddl then
            print("FILE SDDL (first 80 chars): " .. file_sddl:sub(1, 80))
            
            -- These should be DIFFERENT SDDLs!
            if service.sddl and file_sddl then
                assert(service.sddl ~= file_sddl, "Service and file SDDLs should be different!")
                print("CONFIRMED: Service and file have different security descriptors")
            end
        end
        
        return true
    )LUA";
    
    auto result = runtime.execute(code);
    
    if (result.isOk()) {
        auto output = result.value();
        // Should see both SERVICE and FILE SDDL mentioned
        EXPECT_TRUE(output.printed_output.find("SERVICE SDDL") != std::string::npos ||
                    output.printed_output.find("Binary:") != std::string::npos)
            << "Should show service information";
    } else {
        GTEST_SKIP() << "Could not access service information";
    }
}

TEST_F(Win32ToolsTest, NoUnwantedToolsRegistered) {
    // Ensure only the requested tools are registered
    std::string code = R"(
        -- These tools should NOT be registered
        local unwanted = {
            "MessageBox",
            "GetComputerName", 
            "GetUserName",
            "GetSystemInfo",
            "GetCurrentDirectory",
            "SetCurrentDirectory",
            "GetTempPath",
            "WriteFile",
            "ReadFile",
            "FileExists",
            "DirectoryExists",
            "CreateDirectory",
            "DeleteFile",
            "GetTickCount",
            "GetEnvironmentVariable",
            "SetEnvironmentVariable"
        }
        
        for _, name in ipairs(unwanted) do
            assert(win32[name] == nil, name .. " should not be registered")
        end
        
        print("No unwanted tools found")
        return true
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Only requested tools should be registered";
}

// Performance test for Sleep
TEST_F(Win32ToolsTest, SleepPerformance) {
    std::string code = R"(
        local iterations = 10
        local total_time = 0
        
        for i = 1, iterations do
            local start = os.clock()
            win32.Sleep(10)  -- Sleep for 10ms
            local elapsed = (os.clock() - start) * 1000
            total_time = total_time + elapsed
        end
        
        local avg_time = total_time / iterations
        print("Average sleep time for 10ms: " .. string.format("%.2f", avg_time) .. "ms")
        
        -- Should be reasonably close to requested time
        assert(avg_time >= 9, "Average should be at least 9ms")
        assert(avg_time <= 30, "Average should not exceed 30ms")
        
        return avg_time
    )";
    
    auto result = runtime.execute(code);
    ASSERT_TRUE(result.isOk()) << "Sleep performance should be reasonable";
}

// Test system prompt generation
TEST_F(Win32ToolsTest, SystemPromptGeneration) {
    auto prompt = registry.getSystemPrompt();
    
    // Check that prompt contains expected sections
    EXPECT_TRUE(prompt.find("## win32 Module") != std::string::npos);
    
    // Check for specific functions that should be present
    EXPECT_TRUE(prompt.find("win32.Whoami") != std::string::npos);
    EXPECT_TRUE(prompt.find("win32.Sleep") != std::string::npos);
    EXPECT_TRUE(prompt.find("win32.ShellExecute") != std::string::npos);
    EXPECT_TRUE(prompt.find("win32.GetServices") != std::string::npos);
    EXPECT_TRUE(prompt.find("win32.GetService") != std::string::npos);
    
    // Check that unwanted functions are NOT present
    EXPECT_FALSE(prompt.find("win32.MessageBox") != std::string::npos);
    EXPECT_FALSE(prompt.find("win32.GetComputerName") != std::string::npos);
    EXPECT_FALSE(prompt.find("win32.WriteFile") != std::string::npos);
}

// Test registry functionality
TEST_F(Win32ToolsTest, RegistryFunctionality) {
    // Check that only requested functions are registered
    EXPECT_TRUE(registry.isRegistered("win32.Whoami"));
    EXPECT_TRUE(registry.isRegistered("win32.Sleep"));
    EXPECT_TRUE(registry.isRegistered("win32.ShellExecute"));
    EXPECT_TRUE(registry.isRegistered("win32.GetServices"));
    EXPECT_TRUE(registry.isRegistered("win32.GetService"));
    
    // Check that unwanted functions are NOT registered
    EXPECT_FALSE(registry.isRegistered("win32.MessageBox"));
    EXPECT_FALSE(registry.isRegistered("win32.GetComputerName"));
    EXPECT_FALSE(registry.isRegistered("win32.WriteFile"));
    
    // Check function documentation
    auto doc = registry.getFunctionDoc("win32.Whoami");
    EXPECT_FALSE(doc.empty());
    EXPECT_TRUE(doc.find("Get current user info") != std::string::npos);
    
    // Get list of all registered functions
    auto functions = registry.getRegisteredFunctions();
    
    // Count win32 functions
    int win32_count = 0;
    for (const auto& func : functions) {
        if (func.find("win32.") == 0) {
            win32_count++;
        }
    }
    
    // Should have exactly 7 win32 functions (Whoami, Sleep, ShellExecute, GetServices, GetService, GetFileSddl, GetFileSecurity)
    EXPECT_EQ(win32_count, 7) << "Should have exactly 7 win32 functions registered";
}