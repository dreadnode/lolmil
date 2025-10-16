#ifndef WIN32_TOOL_SCHEMAS_HPP
#define WIN32_TOOL_SCHEMAS_HPP

#include "lua_tool_types.hpp"

namespace lua::win32::schemas {

using namespace lua::tools;

// Schema for basic service info returned by GetServices()
inline ObjectSchema getBasicServiceSchema() {
    return {
        "BasicServiceInfo",
        "Basic service information from GetServices()",
        {
            {"name", "string", "Service name (e.g. 'Spooler', 'BITS')", true, "Spooler"},
            {"displayName", "string", "Display name of the service", true, "Print Spooler"},
            {"state", "string", "Current state (RUNNING, STOPPED, etc.)", true, "RUNNING"},
            {"processId", "number", "Process ID if running, 0 if stopped", true, "1234"}
        }
    };
}

// Schema for detailed service info returned by GetService()
inline ObjectSchema getDetailedServiceSchema() {
    return {
        "DetailedServiceInfo",
        "Detailed service information from GetService()",
        {
            {"name", "string", "Service name", true, "Spooler"},
            {"displayName", "string", "Display name", true, "Print Spooler"},
            {"binaryPath", "string", "Path to service executable", true, "C:\\Windows\\System32\\spoolsv.exe"},
            {"description", "string", "Service description", false, "Manages print jobs"},
            {"type", "string", "Service type", true, "WIN32_OWN_PROCESS"},
            {"state", "string", "Current state", true, "RUNNING"},
            {"startType", "string", "Start type (AUTO_START, MANUAL, etc.)", true, "AUTO_START"},
            {"processId", "number", "Process ID if running", true, "1234"},
            {"dependencies", "table", "Array of service dependencies", false},
            {"sddl", "string", "Service security descriptor", false}
        }
    };
}

// Schema for file security info returned by GetFileSecurity()
inline ObjectSchema getFileSecuritySchema() {
    return {
        "FileSecurityInfo",
        "File security information from GetFileSecurity()",
        {
            {"path", "string", "File path that was checked", true, "C:\\Windows\\System32\\spoolsv.exe"},
            {"owner", "string", "Owner of the file", true, "NT AUTHORITY\\SYSTEM"},
            {"owner_sid", "string", "Security ID of the owner", true, "S-1-5-18"},
            {"writable_by_users", "boolean", "TRUE if Users group can write", true, "false"},
            {"writable_by_everyone", "boolean", "TRUE if Everyone group can write", true, "false"}
        }
    };
}

// Schema for user info returned by Whoami()
inline ObjectSchema getWhoamiSchema() {
    return {
        "UserInfo",
        "Current user information from Whoami()",
        {
            {"username", "string", "Current username", true, "Administrator"},
            {"domain", "string", "Domain or computer name", true, "DESKTOP-ABC123"},
            {"groups", "table", "Array of group names user belongs to", true}
        }
    };
}

// Generate full documentation for win32 module
inline std::string getWin32Documentation() {
    std::string doc = R"(## win32 Module - Windows System Functions

### Type Definitions

)";
    
    doc += "#### " + getBasicServiceSchema().toDocString() + "\n";
    doc += "#### " + getDetailedServiceSchema().toDocString() + "\n";
    doc += "#### " + getFileSecuritySchema().toDocString() + "\n";
    doc += "#### " + getWhoamiSchema().toDocString() + "\n";
    
    doc += R"(
### Functions

win32.GetServices() -> table of BasicServiceInfo | nil, error
  Returns array of all Windows services with basic information.
  NOTE: This does NOT include binaryPath - use GetService() for that.
  
  Example:
    local services, err = win32.GetServices()
    if not services then 
      print("Error: " .. err)
      return 
    end
    for _, svc in ipairs(services) do
      print(svc.name .. ": " .. svc.state)
    end

win32.GetService(serviceName) -> DetailedServiceInfo | nil, error
  Returns detailed information for a specific service.
  NOTE: This is the ONLY way to get binaryPath for a service.
  
  Example:
    local info, err = win32.GetService("Spooler")
    if not info then 
      print("Error: " .. err)
      return
    end
    print("Binary: " .. info.binaryPath)

win32.GetFileSecurity(path) -> FileSecurityInfo | nil, error
  Returns file permission information.
  CRITICAL: Use the EXACT field names shown in FileSecurityInfo.
  
  Example:
    local sec, err = win32.GetFileSecurity("C:\\Windows\\System32\\spoolsv.exe")
    if not sec then
      print("Error: " .. err)
      return  
    end
    if sec.writable_by_everyone then  -- MUST use this exact field name
      print("File is writable by everyone!")
    end

win32.Whoami() -> UserInfo
  Returns current user information.
  
  Example:
    local user = win32.Whoami()
    print("User: " .. user.username .. "\\" .. user.domain)

### CRITICAL FIELD NAME RULES

When checking if a file is writable by everyone, you MUST use:
  sec.writable_by_everyone  ✓ CORRECT

NEVER use these (they don't exist):
  sec.full_control_by_everyone  ✗ WRONG
  sec.everyone_writable  ✗ WRONG  
  sec.world_writable  ✗ WRONG

### Common Pattern: Find Services Writable by Everyone

local services, err = win32.GetServices()
if not services then return end

for _, svc in ipairs(services) do
  local info, err = win32.GetService(svc.name)  -- Get binaryPath
  if info and info.binaryPath then
    local sec, err = win32.GetFileSecurity(info.binaryPath)
    if sec and sec.writable_by_everyone then  -- EXACT field name
      print(svc.name .. " binary is writable by everyone")
    end
  end
end
)";
    
    return doc;
}

} // namespace lua::win32::schemas

#endif // WIN32_TOOL_SCHEMAS_HPP