#include "code_extractor.hpp"
#include <regex>
#include <algorithm>
#include <cctype>

std::vector<CodeExtractor::CodeBlock> CodeExtractor::extractCodeBlocks(const std::string& text) {
    std::vector<CodeBlock> blocks;

    // Require both opening and closing backticks to avoid executing truncated code
    std::regex complete_block_regex(R"(```([a-zA-Z0-9_+-]*)\n([\s\S]*?)\n?```)");

    auto blocks_begin = std::sregex_iterator(text.begin(), text.end(), complete_block_regex);
    auto blocks_end = std::sregex_iterator();

    for (std::sregex_iterator i = blocks_begin; i != blocks_end; ++i) {
        std::smatch match = *i;

        CodeBlock block;
        block.language = match[1].str();
        block.code = match[2].str();
        block.start_pos = static_cast<size_t>(match.position());
        block.end_pos = block.start_pos + static_cast<size_t>(match.length());

        if (!block.code.empty() && block.code.back() == '\n') {
            block.code.pop_back();
        }

        blocks.push_back(block);
    }

    // Detect incomplete blocks (no closing backticks) to signal truncated responses
    if (blocks.empty()) {
        std::regex incomplete_block_regex(R"(```([a-zA-Z0-9_+-]*)\n[\s\S]*$)");
        if (std::regex_search(text, incomplete_block_regex)) {
            return blocks;  // Empty vector signals incomplete block
        }
    }

    return blocks;
}

std::vector<CodeExtractor::CodeBlock> CodeExtractor::extractFromText(const std::string& text) {
    return extractCodeBlocks(text);
}

std::vector<CodeExtractor::CodeBlock> CodeExtractor::extractFromLastResponse(const Conversation& conv) {
    const auto& history = conv.getHistory();

    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->role == Message::Assistant) {
            return extractCodeBlocks(it->content);
        }
    }
    
    return {};
}

std::vector<CodeExtractor::CodeBlock> CodeExtractor::extractFromConversation(const Conversation& conv) {
    std::vector<CodeBlock> all_blocks;
    const auto& history = conv.getHistory();

    size_t cumulative_pos = 0;

    for (const auto& msg : history) {
        if (msg.role == Message::Assistant) {
            auto blocks = extractCodeBlocks(msg.content);

            for (auto& block : blocks) {
                block.start_pos += cumulative_pos;
                block.end_pos += cumulative_pos;
                all_blocks.push_back(block);
            }
        }

        cumulative_pos += msg.content.length() + 1;  // +1 for newline between messages
    }
    
    return all_blocks;
}

std::vector<CodeExtractor::CodeBlock> CodeExtractor::filterByLanguage(
    const std::vector<CodeBlock>& blocks, 
    const std::string& language) {
    
    std::vector<CodeBlock> filtered;
    
    std::string lower_lang = language;
    std::transform(lower_lang.begin(), lower_lang.end(), lower_lang.begin(), ::tolower);
    
    for (const auto& block : blocks) {
        std::string block_lang = block.language;
        std::transform(block_lang.begin(), block_lang.end(), block_lang.begin(), ::tolower);
        
        if (block_lang == lower_lang) {
            filtered.push_back(block);
        }
    }
    
    return filtered;
}