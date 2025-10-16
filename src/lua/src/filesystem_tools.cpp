#include "filesystem_tools.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <optional>

namespace lua::filesystem {

namespace fs = std::filesystem;

void registerTools(lua::tools::ToolRegistry& registry, sol::state& state) {
    auto fs_table = state["fs"].get_or_create<sol::table>();
    registry.bind(state, "fs", "writeText",
        "(path, content) -> bool - Write text to file",
        [](const std::string& path, const std::string& content) -> bool {
            std::ofstream file(path);
            if (!file) return false;
            
            file << content;
            return file.good();
        });
}

} // namespace lua::filesystem