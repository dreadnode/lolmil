#ifndef TOKENIZER_JSON_SCHEMA_HPP
#define TOKENIZER_JSON_SCHEMA_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Forward declaration not needed - include json.hpp directly
#include "json.hpp"

// Structured representation of the tokenizer JSON schema
struct TokenizerSchema {
    struct Token {
        std::string content;
        int32_t id;
    };
    
    struct Model {
        std::unordered_map<std::string, int32_t> vocab;
        // Could add merges, type, etc. if needed
    };
    
    Model model;
    std::vector<Token> added_tokens;
    
    // Helper method to merge all tokens into a single map
    void merge_to_maps(std::unordered_map<std::string, int32_t>& token_to_id,
                      std::unordered_map<int32_t, std::string>& id_to_token) const {
        // Add vocab tokens
        for (const auto& [token, id] : model.vocab) {
            token_to_id[token] = id;
            id_to_token[id] = token;
        }
        
        // Add special tokens
        for (const auto& token : added_tokens) {
            token_to_id[token.content] = token.id;
            id_to_token[token.id] = token.content;
        }
    }
};

// Parser class to handle JSON conversion to structured data
class TokenizerJsonParser {
public:
    static TokenizerSchema parse(const std::string& json_content);
    
private:
    static void parse_model(const nlohmann::json& model_obj, 
                          TokenizerSchema::Model& model);
    static void parse_added_tokens(const nlohmann::json& tokens_array,
                                  std::vector<TokenizerSchema::Token>& tokens);
};

#endif // TOKENIZER_JSON_SCHEMA_HPP