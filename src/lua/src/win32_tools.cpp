#include "win32_tools.hpp"
#include "utf_utils.hpp"
#include <Windows.h>
#include <Shellapi.h>
#include <Lmcons.h>
#include <sddl.h>
#include <aclapi.h>
#include <array>
#include <vector>
#include <string>
#include <optional>
#include <tuple>
#include <memory>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")

// Undefine Windows macros that conflict with our function names
#ifdef ShellExecute
#undef ShellExecute
#endif

namespace lua::win32 {

std::wstring toWide(const std::string& utf8) {
    return utf::to_wide(utf8);
}

std::string fromWide(const std::wstring& wide) {
    return utf::to_utf8(wide);
}

class WinHandle {
public:
    explicit WinHandle(HANDLE h = nullptr) : handle(h) {}
    ~WinHandle() { 
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
    WinHandle(WinHandle&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    WinHandle& operator=(WinHandle&& other) noexcept {
        if (this != &other) {
            if (handle && handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    HANDLE get() const { return handle; }
    explicit operator bool() const { return handle && handle != INVALID_HANDLE_VALUE; }
    
private:
    HANDLE handle;
};

class ServiceHandle {
public:
    explicit ServiceHandle(SC_HANDLE h = nullptr) : handle(h) {}
    ~ServiceHandle() {
        if (handle) {
            CloseServiceHandle(handle);
        }
    }
    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle& operator=(const ServiceHandle&) = delete;
    ServiceHandle(ServiceHandle&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    ServiceHandle& operator=(ServiceHandle&& other) noexcept {
        if (this != &other) {
            if (handle) {
                CloseServiceHandle(handle);
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    SC_HANDLE get() const { return handle; }
    explicit operator bool() const { return handle != nullptr; }
    
private:
    SC_HANDLE handle;
};

std::vector<std::string> getUserGroups() {
    std::vector<std::string> groups;

    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        return groups;
    }
    WinHandle token(tokenHandle);
    DWORD tokenInfoLength = 0;
    GetTokenInformation(token.get(), TokenGroups, nullptr, 0, &tokenInfoLength);

    if (tokenInfoLength == 0) {
        return groups;
    }

    std::vector<BYTE> buffer(tokenInfoLength);
    if (!GetTokenInformation(token.get(), TokenGroups, buffer.data(), tokenInfoLength, &tokenInfoLength)) {
        return groups;
    }

    TOKEN_GROUPS* pGroups = reinterpret_cast<TOKEN_GROUPS*>(buffer.data());

    for (DWORD i = 0; i < pGroups->GroupCount; i++) {
        LPWSTR sidString = nullptr;
        if (!ConvertSidToStringSidW(pGroups->Groups[i].Sid, &sidString)) {
            continue;
        }
        std::array<wchar_t, 256> name{};
        std::array<wchar_t, 256> domain{};
        DWORD nameSize = 256;
        DWORD domainSize = 256;
        SID_NAME_USE sidType;
        
        if (LookupAccountSidW(nullptr, pGroups->Groups[i].Sid, 
                              name.data(), &nameSize,
                              domain.data(), &domainSize, &sidType)) {
            std::wstring fullName;
            if (domainSize > 0 && domain[0] != L'\0') {
                fullName = std::wstring(domain.data()) + L"\\" + std::wstring(name.data());
            } else {
                fullName = name.data();
            }
            groups.push_back(fromWide(fullName));
        }
        
        LocalFree(sidString);
    }
    
    return groups;
}

void registerTools(lua::tools::ToolRegistry& registry, sol::state& state) {
    auto win32 = state["win32"].get_or_create<sol::table>();

    registry.bind(state, "win32", "Whoami",
        "() -> table - Get current user info {username, domain, groups}",
        [](sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            sol::table info = lua.create_table();

            std::array<wchar_t, UNLEN + 1> username{};
            DWORD usernameSize = UNLEN + 1;
            if (GetUserNameW(username.data(), &usernameSize)) {
                info["username"] = fromWide(username.data());
            }

            std::array<wchar_t, MAX_COMPUTERNAME_LENGTH + 1> computerName{};
            DWORD computerNameSize = MAX_COMPUTERNAME_LENGTH + 1;
            if (GetComputerNameW(computerName.data(), &computerNameSize)) {
                info["domain"] = fromWide(computerName.data());
            }

            sol::table groupsTable = lua.create_table();
            auto groups = getUserGroups();
            for (size_t i = 0; i < groups.size(); ++i) {
                groupsTable[i + 1] = groups[i];
            }
            info["groups"] = groupsTable;
            
            return info;
        });

    registry.bind(state, "win32", "Sleep",
        "(milliseconds) - Sleep for specified milliseconds",
        [](int milliseconds) {
            if (milliseconds > 0) {
                ::Sleep(static_cast<DWORD>(milliseconds));
            }
        });

    registry.bind(state, "win32", "ShellExecute",
        "(file, operation?, params?, directory?, showCmd?) -> bool, errorCode? - Execute file or open URL",
        [](const std::string& file, 
           std::optional<std::string> operation,
           std::optional<std::string> params,
           std::optional<std::string> directory,
           std::optional<int> showCmd) -> std::tuple<bool, int> {
            
            auto wfile = toWide(file);
            auto woperation = operation ? toWide(*operation) : L"open";
            auto wparams = params ? toWide(*params) : std::wstring();
            auto wdirectory = directory ? toWide(*directory) : std::wstring();
            int nShowCmd = showCmd.value_or(SW_SHOWNORMAL);
            
            HINSTANCE result = ShellExecuteW(
                nullptr,
                operation ? woperation.c_str() : nullptr,
                wfile.c_str(),
                params ? wparams.c_str() : nullptr,
                directory ? wdirectory.c_str() : nullptr,
                nShowCmd
            );
            
            intptr_t resultCode = reinterpret_cast<intptr_t>(result);
            bool success = resultCode > 32;
            
            return std::make_tuple(success, static_cast<int>(resultCode));
        });

    registry.bind(state, "win32", "GetServices",
        "() -> table|nil, error? - List all Windows services with basic info",
        [](sol::this_state s) -> std::tuple<sol::optional<sol::table>, sol::optional<std::string>> {
            sol::state_view lua(s);

            ServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
            if (!scm) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to open service control manager"));
            }

            DWORD bytesNeeded = 0;
            DWORD servicesReturned = 0;
            DWORD resumeHandle = 0;
            
            EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                                  SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded,
                                  &servicesReturned, &resumeHandle, nullptr);
            
            if (bytesNeeded == 0) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to enumerate services"));
            }

            std::vector<BYTE> buffer(bytesNeeded);
            if (!EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                                       SERVICE_STATE_ALL, buffer.data(), bytesNeeded,
                                       &bytesNeeded, &servicesReturned, &resumeHandle, nullptr)) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to enumerate services"));
            }

            sol::table services = lua.create_table();
            ENUM_SERVICE_STATUS_PROCESSW* pServices = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
            
            for (DWORD i = 0; i < servicesReturned; i++) {
                sol::table service = lua.create_table();
                service["name"] = fromWide(pServices[i].lpServiceName);
                service["displayName"] = fromWide(pServices[i].lpDisplayName);

                const char* state = "unknown";
                switch (pServices[i].ServiceStatusProcess.dwCurrentState) {
                    case SERVICE_STOPPED: state = "stopped"; break;
                    case SERVICE_START_PENDING: state = "start_pending"; break;
                    case SERVICE_STOP_PENDING: state = "stop_pending"; break;
                    case SERVICE_RUNNING: state = "running"; break;
                    case SERVICE_CONTINUE_PENDING: state = "continue_pending"; break;
                    case SERVICE_PAUSE_PENDING: state = "pause_pending"; break;
                    case SERVICE_PAUSED: state = "paused"; break;
                }
                service["state"] = state;
                service["processId"] = pServices[i].ServiceStatusProcess.dwProcessId;
                
                services[i + 1] = service;
            }
            
            return std::make_tuple(sol::optional<sol::table>(services), sol::nullopt);
        });

    registry.bind(state, "win32", "GetService",
        "(serviceName) -> table|nil, error? - Get detailed service information including binary path and SERVICE security descriptor (SDDL)",
        [](sol::this_state s, const std::string& serviceName) -> std::tuple<sol::optional<sol::table>, sol::optional<std::string>> {
            sol::state_view lua(s);

            ServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
            if (!scm) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to open service control manager"));
            }

            // READ_CONTROL required for DACL access
            auto wServiceName = toWide(serviceName);
            ServiceHandle service(OpenServiceW(scm.get(), wServiceName.c_str(), 
                                              SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | READ_CONTROL));
            if (!service) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to open service: " + serviceName));
            }

            DWORD bytesNeeded = 0;
            QueryServiceConfigW(service.get(), nullptr, 0, &bytesNeeded);
            
            if (bytesNeeded == 0) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to query service configuration"));
            }
            
            std::vector<BYTE> configBuffer(bytesNeeded);
            LPQUERY_SERVICE_CONFIGW pConfig = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(configBuffer.data());
            
            if (!QueryServiceConfigW(service.get(), pConfig, bytesNeeded, &bytesNeeded)) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to get service configuration"));
            }

            SERVICE_STATUS_PROCESS status;
            DWORD statusBytesNeeded;
            if (!QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
                                     reinterpret_cast<LPBYTE>(&status), sizeof(status),
                                     &statusBytesNeeded)) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to get service status"));
            }

            std::string sddlString;
            PSECURITY_DESCRIPTOR pSecurityDescriptor = nullptr;
            DWORD sdSize = 0;

            if (!QueryServiceObjectSecurity(service.get(), 
                                           DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
                                           nullptr, 0, &sdSize) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> sdBuffer(sdSize);
                pSecurityDescriptor = reinterpret_cast<PSECURITY_DESCRIPTOR>(sdBuffer.data());

                if (QueryServiceObjectSecurity(service.get(),
                                              DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
                                              pSecurityDescriptor, sdSize, &sdSize)) {
                    LPWSTR pSddl = nullptr;
                    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
                            pSecurityDescriptor,
                            SDDL_REVISION_1,
                            DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
                            &pSddl,
                            nullptr)) {
                        sddlString = fromWide(pSddl);
                        LocalFree(pSddl);
                    }
                }
            }
            
            // Build result table
            sol::table info = lua.create_table();
            info["name"] = serviceName;
            info["displayName"] = pConfig->lpDisplayName ? fromWide(pConfig->lpDisplayName) : "";
            info["binaryPath"] = pConfig->lpBinaryPathName ? fromWide(pConfig->lpBinaryPathName) : "";
            info["description"] = pConfig->lpServiceStartName ? fromWide(pConfig->lpServiceStartName) : "";
            
            // Start type
            const char* startType = "unknown";
            switch (pConfig->dwStartType) {
                case SERVICE_BOOT_START: startType = "boot"; break;
                case SERVICE_SYSTEM_START: startType = "system"; break;
                case SERVICE_AUTO_START: startType = "automatic"; break;
                case SERVICE_DEMAND_START: startType = "manual"; break;
                case SERVICE_DISABLED: startType = "disabled"; break;
            }
            info["startType"] = startType;
            
            // Service type
            if (pConfig->dwServiceType & SERVICE_KERNEL_DRIVER) {
                info["type"] = "kernel_driver";
            } else if (pConfig->dwServiceType & SERVICE_FILE_SYSTEM_DRIVER) {
                info["type"] = "file_system_driver";
            } else if (pConfig->dwServiceType & SERVICE_WIN32_OWN_PROCESS) {
                info["type"] = "own_process";
            } else if (pConfig->dwServiceType & SERVICE_WIN32_SHARE_PROCESS) {
                info["type"] = "share_process";
            } else {
                info["type"] = "unknown";
            }
            
            // Current state
            const char* state = "unknown";
            switch (status.dwCurrentState) {
                case SERVICE_STOPPED: state = "stopped"; break;
                case SERVICE_START_PENDING: state = "start_pending"; break;
                case SERVICE_STOP_PENDING: state = "stop_pending"; break;
                case SERVICE_RUNNING: state = "running"; break;
                case SERVICE_CONTINUE_PENDING: state = "continue_pending"; break;
                case SERVICE_PAUSE_PENDING: state = "pause_pending"; break;
                case SERVICE_PAUSED: state = "paused"; break;
            }
            info["state"] = state;
            info["processId"] = status.dwProcessId;
            
            // Add SDDL string if available
            if (!sddlString.empty()) {
                info["sddl"] = sddlString;
            }
            
            // Dependencies
            if (pConfig->lpDependencies) {
                sol::table deps = lua.create_table();
                int depIndex = 1;
                LPCWSTR pDep = pConfig->lpDependencies;
                while (*pDep) {
                    deps[depIndex++] = fromWide(pDep);
                    pDep += wcslen(pDep) + 1;
                }
                info["dependencies"] = deps;
            }
            
            return std::make_tuple(sol::optional<sol::table>(info), sol::nullopt);
        });
    
    // IsServiceVulnerable - Check if a service has privilege escalation vulnerabilities
    registry.bind(state, "win32", "IsServiceVulnerable",
        "(serviceName) -> table|nil, error? - Check for service vulnerabilities (returns vulnerability details or nil)",
        [](sol::this_state s, const std::string& serviceName) -> std::tuple<sol::optional<sol::table>, sol::optional<std::string>> {
            sol::state_view lua(s);

            ServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
            if (!scm) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to open service control manager"));
            }
            
            // Open the service with query config and read control
            auto wServiceName = toWide(serviceName);
            ServiceHandle service(OpenServiceW(scm.get(), wServiceName.c_str(), 
                                              SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | READ_CONTROL));
            if (!service) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Service not found: " + serviceName));
            }
            
            sol::table result = lua.create_table();
            result["service_name"] = serviceName;
            sol::table vulnerabilities = lua.create_table();
            
            // Get service configuration
            DWORD bytesNeeded = 0;
            QueryServiceConfigW(service.get(), nullptr, 0, &bytesNeeded);
            if (bytesNeeded == 0) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to query service configuration"));
            }
            
            std::vector<BYTE> configBuffer(bytesNeeded);
            LPQUERY_SERVICE_CONFIGW pConfig = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(configBuffer.data());
            
            if (!QueryServiceConfigW(service.get(), pConfig, bytesNeeded, &bytesNeeded)) {
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to get service configuration"));
            }
            
            std::wstring binaryPath = pConfig->lpBinaryPathName ? pConfig->lpBinaryPathName : L"";
            std::wstring serviceStartName = pConfig->lpServiceStartName ? pConfig->lpServiceStartName : L"";
            
            // Check if service runs as high privilege account
            bool runsAsSystem = (serviceStartName.empty() || 
                                serviceStartName == L"LocalSystem" ||
                                serviceStartName == L"NT AUTHORITY\\SYSTEM" ||
                                serviceStartName == L"NT AUTHORITY\\LocalService" ||
                                serviceStartName == L"NT AUTHORITY\\NetworkService");
            
            result["runs_as_system"] = runsAsSystem;
            result["binary_path"] = fromWide(binaryPath);
            result["service_account"] = fromWide(serviceStartName);
            
            // === VULNERABILITY CHECK 1: Service ACL allows current user to modify ===
            bool canModifyService = false;
            std::string modifyReason;
            
            // Get current process token
            HANDLE hToken = nullptr;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
                // Get service security descriptor
                PSECURITY_DESCRIPTOR pSD = nullptr;
                DWORD sdSize = 0;
                
                if (!QueryServiceObjectSecurity(service.get(), 
                                               DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                               nullptr, 0, &sdSize) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    std::vector<BYTE> sdBuffer(sdSize);
                    pSD = reinterpret_cast<PSECURITY_DESCRIPTOR>(sdBuffer.data());
                    
                    if (QueryServiceObjectSecurity(service.get(),
                                                  DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                                  pSD, sdSize, &sdSize)) {
                        
                        // Check if current user has SERVICE_CHANGE_CONFIG permission
                        GENERIC_MAPPING genericMapping = {0};
                        genericMapping.GenericRead = SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS;
                        genericMapping.GenericWrite = SERVICE_CHANGE_CONFIG;
                        genericMapping.GenericExecute = SERVICE_START | SERVICE_STOP;
                        genericMapping.GenericAll = SERVICE_ALL_ACCESS;
                        
                        DWORD desiredAccess = SERVICE_CHANGE_CONFIG;
                        DWORD grantedAccess = 0;
                        BOOL accessStatus = FALSE;
                        PRIVILEGE_SET privSet = {0};
                        DWORD privSetSize = sizeof(PRIVILEGE_SET);
                        
                        if (AccessCheck(pSD, hToken, desiredAccess, &genericMapping,
                                       &privSet, &privSetSize, &grantedAccess, &accessStatus)) {
                            if (accessStatus) {
                                canModifyService = true;
                                modifyReason = "Current user has SERVICE_CHANGE_CONFIG permission";
                            }
                        }
                        
                        // Also check for Everyone having modify permissions
                        PACL pDacl = nullptr;
                        BOOL bDaclPresent = FALSE;
                        BOOL bDaclDefaulted = FALSE;
                        
                        if (GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted) && bDaclPresent && pDacl) {
                            ACL_SIZE_INFORMATION aclInfo = {0};
                            if (GetAclInformation(pDacl, &aclInfo, sizeof(aclInfo), AclSizeInformation)) {
                                for (DWORD i = 0; i < aclInfo.AceCount; i++) {
                                    PACE_HEADER pAceHeader = nullptr;
                                    if (GetAce(pDacl, i, reinterpret_cast<LPVOID*>(&pAceHeader))) {
                                        if (pAceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                                            PACCESS_ALLOWED_ACE pAce = reinterpret_cast<PACCESS_ALLOWED_ACE>(pAceHeader);
                                            PSID pSid = reinterpret_cast<PSID>(&pAce->SidStart);
                                            
                                            // Check if this is Everyone (S-1-1-0)
                                            PSID pEveryoneSid = nullptr;
                                            SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
                                            if (AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID,
                                                                        0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
                                                if (EqualSid(pSid, pEveryoneSid)) {
                                                    if (pAce->Mask & SERVICE_CHANGE_CONFIG) {
                                                        canModifyService = true;
                                                        modifyReason = "Everyone has SERVICE_CHANGE_CONFIG permission";
                                                    }
                                                }
                                                FreeSid(pEveryoneSid);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                CloseHandle(hToken);
            }
            
            if (canModifyService && runsAsSystem) {
                vulnerabilities["service_acl"] = modifyReason;
            }
            
            // === VULNERABILITY CHECK 2: Unquoted service path ===
            if (!binaryPath.empty()) {
                // Check if path contains spaces and is not quoted
                bool hasSpace = binaryPath.find(L' ') != std::wstring::npos;
                bool isQuoted = (binaryPath[0] == L'"');
                
                if (hasSpace && !isQuoted) {
                    // Extract just the executable path (before any arguments)
                    std::wstring exePath = binaryPath;
                    size_t argPos = binaryPath.find(L' ');
                    if (argPos != std::wstring::npos && binaryPath[0] != L'"') {
                        // Path is unquoted and has spaces - check each segment
                        std::wstring checkPath;
                        std::vector<std::wstring> writablePaths;
                        
                        size_t pos = 0;
                        while ((pos = binaryPath.find(L' ', pos)) != std::wstring::npos) {
                            checkPath = binaryPath.substr(0, pos);
                            
                            // Check if we can write to the parent directory
                            size_t lastSlash = checkPath.rfind(L'\\');
                            if (lastSlash != std::wstring::npos) {
                                std::wstring parentDir = checkPath.substr(0, lastSlash);
                                
                                // Check write access to parent directory
                                DWORD attributes = GetFileAttributesW(parentDir.c_str());
                                if (attributes != INVALID_FILE_ATTRIBUTES) {
                                    HANDLE hDir = CreateFileW(parentDir.c_str(),
                                                            FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY,
                                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                            nullptr,
                                                            OPEN_EXISTING,
                                                            FILE_FLAG_BACKUP_SEMANTICS,
                                                            nullptr);
                                    if (hDir != INVALID_HANDLE_VALUE) {
                                        CloseHandle(hDir);
                                        writablePaths.push_back(parentDir);
                                    }
                                }
                            }
                            pos++;
                        }
                        
                        if (!writablePaths.empty() && runsAsSystem) {
                            std::string vulnDesc = "Unquoted path with writable directories: ";
                            for (const auto& path : writablePaths) {
                                vulnDesc += fromWide(path) + "; ";
                            }
                            vulnerabilities["unquoted_path"] = vulnDesc;
                        }
                    }
                }
            }
            
            // === VULNERABILITY CHECK 3: Binary file permissions ===
            if (!binaryPath.empty()) {
                // Extract actual file path (remove arguments if present)
                std::wstring filePath = binaryPath;
                if (filePath[0] == L'"') {
                    // Quoted path - extract between quotes
                    size_t endQuote = filePath.find(L'"', 1);
                    if (endQuote != std::wstring::npos) {
                        filePath = filePath.substr(1, endQuote - 1);
                    }
                } else {
                    // Unquoted - take up to first space (if any)
                    size_t spacePos = filePath.find(L' ');
                    if (spacePos != std::wstring::npos) {
                        filePath = filePath.substr(0, spacePos);
                    }
                }
                
                // Check if current user can write to the binary
                HANDLE hFile = CreateFileW(filePath.c_str(),
                                         FILE_WRITE_DATA | FILE_APPEND_DATA,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr,
                                         OPEN_EXISTING,
                                         0,
                                         nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    CloseHandle(hFile);
                    if (runsAsSystem) {
                        vulnerabilities["binary_writable"] = "Current user can write to service binary: " + fromWide(filePath);
                    }
                }
            }
            
            // Check if any vulnerabilities were found
            bool hasVulnerabilities = false;
            for (auto& _ : vulnerabilities) {
                (void)_; // Unused
                hasVulnerabilities = true;
                break;
            }
            
            // Return results
            if (hasVulnerabilities) {
                result["vulnerable"] = true;
                result["vulnerabilities"] = vulnerabilities;
                return std::make_tuple(sol::optional<sol::table>(result), sol::nullopt);
            }
            
            // No vulnerabilities found
            return std::make_tuple(sol::nullopt, sol::nullopt);
        });
    
    // GetFileSddl - Get security descriptor (SDDL) for a file or directory
    registry.bind(state, "win32", "GetFileSddl",
        "(path) -> string|nil, error? - Get file/directory security descriptor (SDDL) string",
        [](const std::string& path) -> std::tuple<sol::optional<std::string>, sol::optional<std::string>> {
            auto widePath = toWide(path);
            
            // Get the security descriptor for the file/directory
            PSECURITY_DESCRIPTOR pSD = nullptr;
            DWORD dwRes = GetNamedSecurityInfoW(
                widePath.c_str(),
                SE_FILE_OBJECT,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                nullptr,  // Owner SID
                nullptr,  // Group SID
                nullptr,  // DACL
                nullptr,  // SACL
                &pSD
            );
            
            if (dwRes != ERROR_SUCCESS) {
                std::string errorMsg = "Failed to get security info: error code " + std::to_string(dwRes);
                return std::make_tuple(sol::nullopt, sol::optional<std::string>(errorMsg));
            }
            
            // Convert to SDDL string
            LPWSTR pSddl = nullptr;
            if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(
                    pSD,
                    SDDL_REVISION_1,
                    OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                    &pSddl,
                    nullptr)) {
                LocalFree(pSD);
                return std::make_tuple(sol::nullopt, sol::optional<std::string>("Failed to convert to SDDL"));
            }
            
            std::string sddlString = fromWide(pSddl);
            LocalFree(pSddl);
            LocalFree(pSD);
            
            return std::make_tuple(sol::optional<std::string>(sddlString), sol::nullopt);
        });
    
    // GetFileSecurity - Get simplified file security info
    registry.bind(state, "win32", "GetFileSecurity", 
        "(path) -> table|nil, error? - Get simplified file permissions (owner, writable_by_users, writable_by_everyone)",
        [](sol::this_state s, const std::string& path) -> std::tuple<sol::optional<sol::table>, sol::optional<std::string>> {
            sol::state_view lua(s);
            
            // Parse the path to extract just the executable (handle paths with arguments)
            std::string cleanPath = path;
            
            // Check if path starts with a quote
            if (!cleanPath.empty() && cleanPath[0] == '"') {
                // Find the closing quote
                size_t endQuote = cleanPath.find('"', 1);
                if (endQuote != std::string::npos) {
                    cleanPath = cleanPath.substr(1, endQuote - 1);
                }
            } else {
                // No quotes, find first space (arguments start there)
                size_t spacePos = cleanPath.find(' ');
                if (spacePos != std::string::npos) {
                    cleanPath = cleanPath.substr(0, spacePos);
                }
            }
            
            auto widePath = toWide(cleanPath);
            
            // Get the security descriptor
            PSECURITY_DESCRIPTOR pSD = nullptr;
            PSID pOwnerSid = nullptr;
            PACL pDacl = nullptr;
            
            DWORD dwRes = GetNamedSecurityInfoW(
                widePath.c_str(),
                SE_FILE_OBJECT,
                OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                &pOwnerSid,
                nullptr,  // Group SID
                &pDacl,
                nullptr,  // SACL
                &pSD
            );
            
            if (dwRes != ERROR_SUCCESS) {
                std::string errorMsg = "Failed to get security info: error code " + std::to_string(dwRes);
                return std::make_tuple(sol::nullopt, sol::optional<std::string>(errorMsg));
            }
            
            sol::table result = lua.create_table();
            
            // Get owner name
            if (pOwnerSid) {
                WCHAR name[256] = {0};
                WCHAR domain[256] = {0};
                DWORD nameSize = 256;
                DWORD domainSize = 256;
                SID_NAME_USE sidType;
                
                if (LookupAccountSidW(nullptr, pOwnerSid, name, &nameSize, domain, &domainSize, &sidType)) {
                    std::wstring owner = domainSize > 0 ? std::wstring(domain) + L"\\" + name : name;
                    result["owner"] = fromWide(owner);
                }
                
                // Convert owner SID to string
                LPWSTR pSidStr = nullptr;
                if (ConvertSidToStringSidW(pOwnerSid, &pSidStr)) {
                    result["owner_sid"] = fromWide(pSidStr);
                    LocalFree(pSidStr);
                }
            }
            
            // Check for write permissions
            bool writableByUsers = false;
            bool writableByEveryone = false;
            
            if (pDacl) {
                ACL_SIZE_INFORMATION aclInfo;
                if (GetAclInformation(pDacl, &aclInfo, sizeof(aclInfo), AclSizeInformation)) {
                    for (DWORD i = 0; i < aclInfo.AceCount; i++) {
                        PACE_HEADER pAce = nullptr;
                        if (GetAce(pDacl, i, (LPVOID*)&pAce)) {
                            if (pAce->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                                ACCESS_ALLOWED_ACE* pAllowedAce = (ACCESS_ALLOWED_ACE*)pAce;
                                
                                // Check for write permissions
                                if (pAllowedAce->Mask & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)) {
                                    // Check if it's the Users group or Everyone
                                    PSID pSid = &pAllowedAce->SidStart;
                                    
                                    // Check for well-known SIDs
                                    WELL_KNOWN_SID_TYPE wellKnownSids[] = {
                                        WinBuiltinUsersSid,      // BUILTIN\Users
                                        WinAuthenticatedUserSid,  // Authenticated Users
                                        WinWorldSid              // Everyone
                                    };
                                    
                                    for (auto sidType : wellKnownSids) {
                                        PSID pWellKnownSid = nullptr;
                                        DWORD sidSize = SECURITY_MAX_SID_SIZE;
                                        BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
                                        pWellKnownSid = (PSID)sidBuffer;
                                        
                                        if (CreateWellKnownSid(sidType, nullptr, pWellKnownSid, &sidSize)) {
                                            if (EqualSid(pSid, pWellKnownSid)) {
                                                if (sidType == WinWorldSid) {
                                                    writableByEveryone = true;
                                                } else {
                                                    writableByUsers = true;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            result["writable_by_users"] = writableByUsers;
            result["writable_by_everyone"] = writableByEveryone;
            result["path"] = path;
            
            LocalFree(pSD);
            
            return std::make_tuple(sol::optional<sol::table>(result), sol::nullopt);
        });
    
    // ModifyService - Change service binary path configuration
    registry.bind(state, "win32", "ModifyService",
        "(serviceName, binaryPath) -> bool, error? - Modify service binary path",
        [](const std::string& serviceName, const std::string& binaryPath) -> std::tuple<bool, sol::optional<std::string>> {
            // Open service control manager with modification rights
            ServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
            if (!scm) {
                return std::make_tuple(false, sol::optional<std::string>("Failed to open service control manager"));
            }
            
            // Open the service with change config permission
            auto wServiceName = toWide(serviceName);
            ServiceHandle service(OpenServiceW(scm.get(), wServiceName.c_str(), SERVICE_CHANGE_CONFIG));
            if (!service) {
                return std::make_tuple(false, sol::optional<std::string>("Failed to open service or access denied: " + serviceName));
            }
            
            // Convert binary path to wide string
            auto wBinaryPath = toWide(binaryPath);
            
            // Change the service configuration
            if (!ChangeServiceConfigW(
                service.get(),
                SERVICE_NO_CHANGE,     // Service type - don't change
                SERVICE_NO_CHANGE,     // Start type - don't change
                SERVICE_NO_CHANGE,     // Error control - don't change
                wBinaryPath.c_str(),   // Binary path - UPDATE THIS
                nullptr,               // Load order group - don't change
                nullptr,               // Tag ID - don't change
                nullptr,               // Dependencies - don't change
                nullptr,               // Service start name - don't change
                nullptr,               // Password - don't change
                nullptr                // Display name - don't change
            )) {
                DWORD error = GetLastError();
                std::string errorMsg = "Failed to modify service: error code " + std::to_string(error);
                return std::make_tuple(false, sol::optional<std::string>(errorMsg));
            }
            
            return std::make_tuple(true, sol::nullopt);
        });
    
    // RestartService - Stop and start a service
    registry.bind(state, "win32", "RestartService", 
        "(serviceName) -> bool, error? - Stop and restart a service",
        [](const std::string& serviceName) -> std::tuple<bool, sol::optional<std::string>> {
            // Open service control manager
            ServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
            if (!scm) {
                return std::make_tuple(false, sol::optional<std::string>("Failed to open service control manager"));
            }
            
            // Open the service with start/stop permissions
            auto wServiceName = toWide(serviceName);
            ServiceHandle service(OpenServiceW(scm.get(), wServiceName.c_str(), 
                                              SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS));
            if (!service) {
                return std::make_tuple(false, sol::optional<std::string>("Failed to open service or access denied: " + serviceName));
            }
            
            // Query current status
            SERVICE_STATUS status;
            if (!QueryServiceStatus(service.get(), &status)) {
                return std::make_tuple(false, sol::optional<std::string>("Failed to query service status"));
            }
            
            // Stop the service if it's running
            if (status.dwCurrentState != SERVICE_STOPPED) {
                if (!ControlService(service.get(), SERVICE_CONTROL_STOP, &status)) {
                    DWORD error = GetLastError();
                    if (error != ERROR_SERVICE_NOT_ACTIVE) {
                        std::string errorMsg = "Failed to stop service: error code " + std::to_string(error);
                        return std::make_tuple(false, sol::optional<std::string>(errorMsg));
                    }
                }
                
                // Wait for service to stop (max 30 seconds)
                int waitCount = 0;
                while (waitCount < 30) {
                    Sleep(1000);  // Wait 1 second
                    if (!QueryServiceStatus(service.get(), &status)) {
                        return std::make_tuple(false, sol::optional<std::string>("Failed to query service status while stopping"));
                    }
                    if (status.dwCurrentState == SERVICE_STOPPED) {
                        break;
                    }
                    waitCount++;
                }
                
                if (status.dwCurrentState != SERVICE_STOPPED) {
                    return std::make_tuple(false, sol::optional<std::string>("Service failed to stop within timeout"));
                }
            }
            
            // Start the service
            if (!StartServiceW(service.get(), 0, nullptr)) {
                DWORD error = GetLastError();
                std::string errorMsg = "Failed to start service: error code " + std::to_string(error);
                return std::make_tuple(false, sol::optional<std::string>(errorMsg));
            }
            
            // Wait for service to start (max 30 seconds)
            int waitCount = 0;
            while (waitCount < 30) {
                Sleep(1000);  // Wait 1 second
                if (!QueryServiceStatus(service.get(), &status)) {
                    return std::make_tuple(false, sol::optional<std::string>("Failed to query service status while starting"));
                }
                if (status.dwCurrentState == SERVICE_RUNNING) {
                    return std::make_tuple(true, sol::nullopt);
                }
                waitCount++;
            }
            
            return std::make_tuple(false, sol::optional<std::string>("Service failed to start within timeout"));
        });
}

// Global flag for agent completion
static bool g_agentComplete = false;
static std::string g_agentMessage;

bool isAgentComplete() {
    return g_agentComplete;
}

std::string getAgentMessage() {
    return g_agentMessage;
}

void resetAgentState() {
    g_agentComplete = false;
    g_agentMessage.clear();
}

void registerAgentControl(lua::tools::ToolRegistry& registry, sol::state& state) {
    // EndAgent - Signal agent completion
    registry.bind(state, "win32", "EndAgent",
        "(message) -> nil - Complete the agent task and exit agent loop",
        [](const std::string& message) {
            g_agentComplete = true;
            g_agentMessage = message;
            // Print the message so user sees it
            std::cout << "[AGENT COMPLETE] " << message << std::endl;
        });
}

} // namespace lua::win32