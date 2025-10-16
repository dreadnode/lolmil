#ifndef FILESYSTEM_TOOLS_HPP
#define FILESYSTEM_TOOLS_HPP

#include <sol/sol.hpp>
#include <string>
#include "lua_tools.hpp"

namespace lua::filesystem {

// Register all filesystem tools with the registry
void registerTools(lua::tools::ToolRegistry& registry, sol::state& state);

} // namespace lua::filesystem

#endif // FILESYSTEM_TOOLS_HPP