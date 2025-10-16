#ifndef LUA_EXTRACT_HPP
#define LUA_EXTRACT_HPP

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <regex>
#include <algorithm>
#include <ranges>

namespace lua::extract {

// Code block structure
struct CodeBlock {
    std::string language;
    std::string code;
    size_t start_pos;
    size_t end_pos;
};

// Extract all code blocks from text
[[nodiscard]] inline std::vector<CodeBlock> extractCodeBlocks(std::string_view text) {
    std::vector<CodeBlock> blocks;
    std::string text_str(text);
    
    // Pattern for fenced code blocks with optional language
    std::regex code_block_regex(R"(```(\w*)\s*\n([\s\S]*?)```)");
    std::sregex_iterator it(text_str.begin(), text_str.end(), code_block_regex);
    std::sregex_iterator end;
    
    while (it != end) {
        CodeBlock block;
        block.language = (*it)[1].str();
        block.code = (*it)[2].str();
        block.start_pos = it->position();
        block.end_pos = block.start_pos + it->length();
        blocks.push_back(std::move(block));
        ++it;
    }
    
    return blocks;
}

// Extract Lua code specifically
[[nodiscard]] inline std::optional<std::string> extractLuaCode(std::string_view text) {
    // First try to find explicitly marked Lua blocks
    auto blocks = extractCodeBlocks(text);
    
    // Filter for Lua blocks
    auto lua_blocks = blocks | std::views::filter([](const CodeBlock& b) {
        return b.language == "lua" || b.language == "Lua" || b.language == "LUA";
    });
    
    // Return first Lua block if found
    for (const auto& block : lua_blocks) {
        return block.code;
    }
    
    // Try generic code blocks and check if they look like Lua
    auto generic_blocks = blocks | std::views::filter([](const CodeBlock& b) {
        return b.language.empty();
    });
    
    for (const auto& block : generic_blocks) {
        // Check for Lua keywords
        const std::string& code = block.code;
        int lua_indicators = 0;
        
        // Common Lua patterns
        if (code.find("function") != std::string::npos) lua_indicators++;
        if (code.find("local") != std::string::npos) lua_indicators++;
        if (code.find("end") != std::string::npos) lua_indicators++;
        if (code.find("then") != std::string::npos) lua_indicators++;
        if (code.find("elseif") != std::string::npos) lua_indicators++;
        if (code.find("return") != std::string::npos) lua_indicators++;
        if (code.find("for") != std::string::npos && 
            code.find("do") != std::string::npos) lua_indicators++;
        
        if (lua_indicators >= 2) {
            return block.code;
        }
    }
    
    // Look for inline Lua patterns
    std::string text_str(text);
    
    // Pattern: "Here's Lua code:" followed by code
    std::regex intro_pattern(
        R"((?:Here's?|This is|The following is).*?(?:Lua|lua|script|code).*?:\s*\n([\s\S]+?)(?:\n\n|\n(?:This|The|Now)|$))",
        std::regex::icase
    );
    std::smatch match;
    
    if (std::regex_search(text_str, match, intro_pattern)) {
        std::string potential_code = match[1].str();
        // Basic validation
        if (potential_code.find('(') != std::string::npos &&
            potential_code.find(')') != std::string::npos) {
            return potential_code;
        }
    }
    
    // Check if entire text looks like Lua
    std::regex lua_start(R"(^\s*(?:local\s+|function\s+|for\s+|while\s+|if\s+|--|\w+\s*=|\w+\())");
    if (std::regex_search(text_str, lua_start)) {
        // Count Lua indicators in full text
        int indicators = 0;
        if (text.find("function") != std::string::npos) indicators++;
        if (text.find("local") != std::string::npos) indicators++;
        if (text.find("end") != std::string::npos) indicators++;
        if (text.find("then") != std::string::npos) indicators++;
        if (text.find("print") != std::string::npos) indicators++;
        
        if (indicators >= 2) {
            // Clean up obvious non-code parts
            std::string code = text_str;
            
            // Remove common prefixes
            std::regex prefix_removal(R"(^[\s\S]*?(?:code:|script:|following:|example:)\s*\n)", 
                                     std::regex::icase);
            code = std::regex_replace(code, prefix_removal, "");
            
            // Remove common suffixes  
            std::regex suffix_removal(R"(\n\s*(?:This (?:code|script)|The (?:code|script)|Note:)[\s\S]*$)", 
                                     std::regex::icase);
            code = std::regex_replace(code, suffix_removal, "");
            
            // Trim whitespace
            auto trim_start = code.find_first_not_of(" \n\r\t");
            auto trim_end = code.find_last_not_of(" \n\r\t");
            
            if (trim_start != std::string::npos && trim_end != std::string::npos) {
                return code.substr(trim_start, trim_end - trim_start + 1);
            }
        }
    }
    
    return std::nullopt;
}

// Extract all Lua blocks (for multiple scripts in one text)
[[nodiscard]] inline std::vector<std::string> extractAllLuaCode(std::string_view text) {
    std::vector<std::string> lua_scripts;
    auto blocks = extractCodeBlocks(text);
    
    for (const auto& block : blocks) {
        // Check if it's Lua or looks like Lua
        if (block.language == "lua" || block.language == "Lua" || block.language == "LUA") {
            lua_scripts.push_back(block.code);
        } else if (block.language.empty()) {
            // Check if it looks like Lua
            const std::string& code = block.code;
            int lua_indicators = 0;
            
            if (code.find("function") != std::string::npos) lua_indicators++;
            if (code.find("local") != std::string::npos) lua_indicators++;
            if (code.find("end") != std::string::npos) lua_indicators++;
            if (code.find("then") != std::string::npos) lua_indicators++;
            
            if (lua_indicators >= 2) {
                lua_scripts.push_back(block.code);
            }
        }
    }
    
    return lua_scripts;
}

// Analyze code for potential issues
struct CodeAnalysis {
    bool has_loops = false;
    bool has_io_operations = false;
    bool has_require_statements = false;
    bool has_loadstring = false;
    bool has_dofile = false;
    std::vector<std::string> defined_functions;
    std::vector<std::string> global_writes;
    size_t estimated_complexity = 0;
};

[[nodiscard]] inline CodeAnalysis analyzeLuaCode(std::string_view code) {
    CodeAnalysis analysis;
    std::string code_str(code);
    
    // Check for loops
    if (code.find("for ") != std::string::npos || 
        code.find("while ") != std::string::npos ||
        code.find("repeat ") != std::string::npos) {
        analysis.has_loops = true;
    }
    
    // Check for IO operations
    if (code.find("io.") != std::string::npos ||
        code.find("file:") != std::string::npos) {
        analysis.has_io_operations = true;
    }
    
    // Check for require statements
    if (code.find("require") != std::string::npos) {
        analysis.has_require_statements = true;
    }
    
    // Check for dynamic code loading
    if (code.find("loadstring") != std::string::npos ||
        code.find("load(") != std::string::npos) {
        analysis.has_loadstring = true;
    }
    
    if (code.find("dofile") != std::string::npos) {
        analysis.has_dofile = true;
    }
    
    // Find function definitions
    std::regex func_regex(R"(function\s+(\w+)\s*\()");
    std::sregex_iterator it(code_str.begin(), code_str.end(), func_regex);
    std::sregex_iterator end;
    
    while (it != end) {
        analysis.defined_functions.push_back((*it)[1].str());
        ++it;
    }
    
    // Find global variable assignments (simplified)
    std::regex global_regex(R"(^(?!local\s+)(\w+)\s*=)");
    it = std::sregex_iterator(code_str.begin(), code_str.end(), global_regex);
    
    while (it != end) {
        std::string var_name = (*it)[1].str();
        // Filter out likely false positives
        if (var_name != "if" && var_name != "then" && var_name != "else" &&
            var_name != "end" && var_name != "function") {
            analysis.global_writes.push_back(var_name);
        }
        ++it;
    }
    
    // Estimate complexity (simple heuristic)
    analysis.estimated_complexity = 
        std::count(code.begin(), code.end(), '\n') +
        analysis.defined_functions.size() * 5 +
        (analysis.has_loops ? 10 : 0) +
        (analysis.has_io_operations ? 5 : 0);
    
    return analysis;
}

} // namespace lua::extract

#endif // LUA_EXTRACT_HPP