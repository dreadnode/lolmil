#ifndef WIN32_TOOLS_HPP
#define WIN32_TOOLS_HPP

#include <sol/sol.hpp>
#include <string>
#include "lua_tools.hpp"

namespace lua::win32 {

// Register all Win32 tools with the registry
void registerTools(lua::tools::ToolRegistry& registry, sol::state& state);

// Register agent control functions
void registerAgentControl(lua::tools::ToolRegistry& registry, sol::state& state);

// Agent state management
bool isAgentComplete();
std::string getAgentMessage();
void resetAgentState();

// UTF conversion utilities (exposed for testing if needed)
std::wstring toWide(const std::string& utf8);
std::string fromWide(const std::wstring& wide);

} // namespace lua::win32

#endif // WIN32_TOOLS_HPP