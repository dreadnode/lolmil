#ifndef LUA_TOOL_TYPES_HPP
#define LUA_TOOL_TYPES_HPP

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace lua::tools {

// Type information for a field in a returned object
struct FieldInfo {
    std::string name;
    std::string type;  // "string", "boolean", "number", "table"
    std::string description;
    bool required = true;
    std::optional<std::string> example;
};

// Type information for a returned object
struct ObjectSchema {
    std::string name;  // e.g. "ServiceInfo", "FileSecurity"
    std::string description;
    std::vector<FieldInfo> fields;
    
    // Generate documentation for this object type
    [[nodiscard]] std::string toDocString() const {
        std::string doc = name + " object:\n";
        for (const auto& field : fields) {
            doc += "  " + field.name + " (" + field.type + "): " + field.description;
            if (!field.required) doc += " [optional]";
            if (field.example) doc += " - e.g. " + *field.example;
            doc += "\n";
        }
        return doc;
    }
};

// Enhanced function documentation with type information
struct FunctionDoc {
    std::string module;
    std::string name;
    std::string description;
    std::vector<std::pair<std::string, std::string>> parameters;  // name, description
    std::string returns;  // return type description
    std::optional<ObjectSchema> returnSchema;  // structured return type
    std::vector<std::string> examples;
    
    // Generate full documentation string
    [[nodiscard]] std::string toDocString() const {
        std::string doc = module + "." + name + "(";
        
        // Parameters
        bool first = true;
        for (const auto& [pname, pdesc] : parameters) {
            if (!first) doc += ", ";
            doc += pname;
            first = false;
        }
        doc += ") -> " + returns + "\n";
        doc += "  " + description + "\n";
        
        // Parameter descriptions
        if (!parameters.empty()) {
            doc += "  Parameters:\n";
            for (const auto& [pname, pdesc] : parameters) {
                doc += "    " + pname + ": " + pdesc + "\n";
            }
        }
        
        // Return schema if available
        if (returnSchema) {
            doc += "  Returns " + returnSchema->toDocString();
        }
        
        // Examples
        if (!examples.empty()) {
            doc += "  Examples:\n";
            for (const auto& ex : examples) {
                doc += "    " + ex + "\n";
            }
        }
        
        return doc;
    }
};

} // namespace lua::tools

#endif // LUA_TOOL_TYPES_HPP