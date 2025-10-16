#ifndef CODE_EXTRACTOR_HPP
#define CODE_EXTRACTOR_HPP

#include <string>
#include <vector>
#include "conversation.hpp"

class CodeExtractor {
public:
    struct CodeBlock {
        std::string language;
        std::string code;
        size_t start_pos;
        size_t end_pos;
    };
    
    [[nodiscard]] static std::vector<CodeBlock> extractFromText(const std::string& text);
    [[nodiscard]] static std::vector<CodeBlock> extractFromLastResponse(const Conversation& conv);
    [[nodiscard]] static std::vector<CodeBlock> extractFromConversation(const Conversation& conv);
    [[nodiscard]] static std::vector<CodeBlock> filterByLanguage(
        const std::vector<CodeBlock>& blocks, 
        const std::string& language);
    
private:
    [[nodiscard]] static std::vector<CodeBlock> extractCodeBlocks(const std::string& text);
};

#endif // CODE_EXTRACTOR_HPP