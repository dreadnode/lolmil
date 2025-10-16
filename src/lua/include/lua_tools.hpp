#ifndef LUA_TOOLS_HPP
#define LUA_TOOLS_HPP

#include <sol/sol.hpp>
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace lua::tools {

// Tool registry for managing function bindings and documentation
class ToolRegistry {
private:
    std::vector<std::string> documentation;
    std::map<std::string, sol::object> functions;  // Store for testing access
    
public:
    // Register a function with its documentation
    template<typename Func>
    void bind(sol::state& state, 
              const std::string& module,
              const std::string& name,
              const std::string& doc,
              Func&& func) {
        // Bind the function to Lua
        auto mod_table = state[module];
        if (!mod_table.valid()) {
            mod_table = state.create_table();
            state[module] = mod_table;
        }
        mod_table[name] = std::forward<Func>(func);
        
        // Store reference for testing
        functions[module + "." + name] = mod_table[name];
        
        // Store documentation
        documentation.push_back("- " + module + "." + name + ": " + doc);
    }
    
    // Get function for testing
    [[nodiscard]] sol::object getFunction(const std::string& full_name) const {
        auto it = functions.find(full_name);
        return it != functions.end() ? it->second : sol::nil;
    }
    
    // Generate system prompt from all registered tools
    [[nodiscard]] std::string getSystemPrompt() const {
        std::stringstream ss;
        ss << "The Lua environment has access to the following functions:\n\n";
        
        // Group by module
        std::map<std::string, std::vector<std::string>> by_module;
        for (const auto& doc : documentation) {
            // Extract module name from "- module.function: description"
            auto dot_pos = doc.find('.');
            if (dot_pos != std::string::npos) {
                std::string module = doc.substr(2, dot_pos - 2);  // Skip "- "
                by_module[module].push_back(doc);
            }
        }
        
        // Output grouped documentation
        for (const auto& [module, docs] : by_module) {
            ss << "## " << module << " Module\n";
            for (const auto& doc : docs) {
                ss << doc << "\n";
            }
            ss << "\n";
        }
        
        return ss.str();
    }
    
    // Get list of all registered functions
    [[nodiscard]] std::vector<std::string> getRegisteredFunctions() const {
        std::vector<std::string> result;
        for (const auto& [name, _] : functions) {
            result.push_back(name);
        }
        return result;
    }
    
    // Check if a function is registered
    [[nodiscard]] bool isRegistered(const std::string& full_name) const {
        return functions.find(full_name) != functions.end();
    }
    
    // Get documentation for a specific function
    [[nodiscard]] std::string getFunctionDoc(const std::string& full_name) const {
        for (const auto& doc : documentation) {
            if (doc.find(full_name + ":") != std::string::npos) {
                return doc;
            }
        }
        return "";
    }
    
    // Clear all registrations (useful for testing)
    void clear() {
        documentation.clear();
        functions.clear();
    }
};

} // namespace lua::tools

#endif // LUA_TOOLS_HPP