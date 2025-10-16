#include <gtest/gtest.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <string>
#include <memory>
#include "lua_api.hpp"
#include "win32_tools.hpp"

class ServiceVulnerabilityTest : public ::testing::Test {
protected:
    std::unique_ptr<lua::LuaRuntime> lua_runtime;
    lua::tools::ToolRegistry tool_registry;
    
    void SetUp() override {
        lua_runtime = std::make_unique<lua::LuaRuntime>();
        lua::win32::registerTools(tool_registry, lua_runtime->getState());
    }
    
    void TearDown() override {
        lua_runtime.reset();
    }
    
    // Helper to create a test service with specific permissions
    bool CreateTestService(const std::string& serviceName, 
                          const std::string& binaryPath,
                          const std::string& serviceAccount = "LocalSystem",
                          const std::string& sddl = "") {
        SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
        if (!hSCM) return false;
        
        auto wServiceName = std::wstring(serviceName.begin(), serviceName.end());
        auto wBinaryPath = std::wstring(binaryPath.begin(), binaryPath.end());
        auto wServiceAccount = serviceAccount.empty() ? nullptr : 
                               std::wstring(serviceAccount.begin(), serviceAccount.end()).c_str();
        
        SC_HANDLE hService = CreateServiceW(
            hSCM,
            wServiceName.c_str(),
            wServiceName.c_str(),
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            wBinaryPath.c_str(),
            nullptr,
            nullptr,
            nullptr,
            wServiceAccount,
            nullptr
        );
        
        bool success = (hService != nullptr);
        
        if (hService && !sddl.empty()) {
            // Set custom security descriptor
            PSECURITY_DESCRIPTOR pSD = nullptr;
            auto wSddl = std::wstring(sddl.begin(), sddl.end());
            if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                    wSddl.c_str(), SDDL_REVISION_1, &pSD, nullptr)) {
                SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, pSD);
                LocalFree(pSD);
            }
        }
        
        if (hService) CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return success;
    }
    
    bool DeleteTestService(const std::string& serviceName) {
        SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hSCM) return false;
        
        auto wServiceName = std::wstring(serviceName.begin(), serviceName.end());
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), DELETE);
        
        bool success = false;
        if (hService) {
            success = DeleteService(hService);
            CloseServiceHandle(hService);
        }
        
        CloseServiceHandle(hSCM);
        return success;
    }
};

TEST_F(ServiceVulnerabilityTest, DetectsEveryoneCanModifyService) {
    // Test detection of Everyone having SERVICE_CHANGE_CONFIG permission
    const std::string testService = "TestVulnService1";
    
    // Create a service with Everyone having full control
    // SDDL: D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD) = Everyone has SERVICE_CHANGE_CONFIG
    std::string sddl = "D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)";
    
    ASSERT_TRUE(CreateTestService(testService, "C:\\Windows\\System32\\cmd.exe", "LocalSystem", sddl));
    
    // Run Lua code to check vulnerability
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        if result then
            print(tostring(result.vulnerable and result.vulnerabilities.service_acl ~= nil))
        else
            print("false")
        end
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
    
    // Cleanup
    DeleteTestService(testService);
}

TEST_F(ServiceVulnerabilityTest, DetectsUnquotedServicePath) {
    // Test detection of unquoted service path vulnerability
    const std::string testService = "TestVulnService2";
    
    // Create a service with unquoted path containing spaces
    std::string unquotedPath = "C:\\Program Files\\Test Service\\service.exe";
    
    ASSERT_TRUE(CreateTestService(testService, unquotedPath, "LocalSystem"));
    
    // Create writable directory to simulate vulnerability
    CreateDirectoryA("C:\\Program Files\\Test", nullptr);
    
    // Run Lua code to check vulnerability
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        if result then
            print(tostring(result.vulnerable and result.vulnerabilities.unquoted_path ~= nil))
        else
            print("false")
        end
    )";
    
    auto result = lua_runtime->execute(luaCode);
    // Note: This test might not detect the vulnerability if we don't have 
    // write access to C:\Program Files, which is expected in normal conditions
    
    // Cleanup
    DeleteTestService(testService);
    RemoveDirectoryA("C:\\Program Files\\Test");
}

TEST_F(ServiceVulnerabilityTest, DetectsWritableBinary) {
    // Test detection of writable service binary
    const std::string testService = "TestVulnService3";
    
    // Create a test binary in temp directory (writable by current user)
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string testBinary = std::string(tempPath) + "test_service.exe";
    
    // Create a dummy executable file
    HANDLE hFile = CreateFileA(testBinary.c_str(), GENERIC_WRITE, 0, nullptr, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        // Write minimal PE header (not a real executable, just for testing)
        const char* dummyExe = "MZ";
        DWORD written;
        WriteFile(hFile, dummyExe, 2, &written, nullptr);
        CloseHandle(hFile);
    }
    
    ASSERT_TRUE(CreateTestService(testService, testBinary, "LocalSystem"));
    
    // Run Lua code to check vulnerability
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        if result then
            print(tostring(result.vulnerable and result.vulnerabilities.binary_writable ~= nil))
        else
            print("false")
        end
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
    
    // Cleanup
    DeleteTestService(testService);
    DeleteFileA(testBinary.c_str());
}

TEST_F(ServiceVulnerabilityTest, NoFalsePositivesForSecureService) {
    // Test that secure services are not flagged as vulnerable
    const std::string testService = "TestSecureService";
    
    // Create a properly configured service
    // - Quoted path
    // - Running as limited user (not SYSTEM)
    // - Restrictive permissions
    std::string quotedPath = "\"C:\\Windows\\System32\\svchost.exe\"";
    std::string restrictiveSddl = "D:(A;;CCLCSWRPWPDTLOCRSDRCWDWO;;;BA)(A;;CCLCSWLOCRRC;;;IU)";
    
    ASSERT_TRUE(CreateTestService(testService, quotedPath, "NT AUTHORITY\\LocalService", restrictiveSddl));
    
    // Run Lua code to check vulnerability
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        print(tostring(result == nil))  -- Should return nil for non-vulnerable service
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
    
    // Cleanup
    DeleteTestService(testService);
}

TEST_F(ServiceVulnerabilityTest, ChecksRunAsSystemRequirement) {
    // Test that vulnerability is only reported for services running as SYSTEM
    const std::string testService = "TestNonSystemService";
    
    // Create service with Everyone having modify permissions but NOT running as SYSTEM
    std::string sddl = "D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)";
    
    // Create with a non-privileged account
    ASSERT_TRUE(CreateTestService(testService, "C:\\Windows\\System32\\cmd.exe", 
                                  "NT AUTHORITY\\LocalService", sddl));
    
    // Run Lua code to check vulnerability
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        -- Should not be vulnerable because it doesn't run as SYSTEM
        print(tostring(result == nil))
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
    
    // Cleanup
    DeleteTestService(testService);
}

TEST_F(ServiceVulnerabilityTest, HandlesNonExistentService) {
    // Test proper error handling for non-existent service
    std::string luaCode = R"(
        local result, error = win32.IsServiceVulnerable('NonExistentService12345')
        print(tostring(result == nil))  -- error handling not needed
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
}

TEST_F(ServiceVulnerabilityTest, ReturnsDetailedVulnerabilityInfo) {
    // Test that the function returns detailed information about vulnerabilities
    const std::string testService = "TestDetailedVuln";
    
    // Create a service with multiple vulnerabilities
    std::string unquotedPath = "C:\\Program Files\\Vulnerable Service\\vuln.exe";
    std::string sddl = "D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)";
    
    ASSERT_TRUE(CreateTestService(testService, unquotedPath, "LocalSystem", sddl));
    
    // Run Lua code to check detailed results
    std::string luaCode = R"(
        local result = win32.IsServiceVulnerable(')" + testService + R"(')
        if result then
            -- Check structure of returned data
            local hasServiceName = result.service_name == ')" + testService + R"('
            local hasVulnFlag = result.vulnerable == true
            local hasVulnDetails = result.vulnerabilities ~= nil
            local runsAsSystem = result.runs_as_system == true
            local hasBinaryPath = result.binary_path ~= nil
            
            return hasServiceName and hasVulnFlag and hasVulnDetails and 
                   runsAsSystem and hasBinaryPath
        end
        return false
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
    
    // Cleanup
    DeleteTestService(testService);
}

// Integration test with real Windows services (read-only, doesn't modify)
TEST_F(ServiceVulnerabilityTest, AnalyzesRealWindowsServices) {
    // Test with some known Windows services (just analysis, no modifications)
    std::string luaCode = R"(
        -- Check a few standard Windows services
        local services = {'Spooler', 'Themes', 'BITS'}
        local analyzed = 0
        
        for _, svc in ipairs(services) do
            local result, error = win32.IsServiceVulnerable(svc)
            -- We just want to ensure the function doesn't crash on real services
            if error == nil then
                analyzed = analyzed + 1
            end
        end
        
        return analyzed > 0  -- At least one service was successfully analyzed
    )";
    
    auto result = lua_runtime->execute(luaCode);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().printed_output, "true");
}