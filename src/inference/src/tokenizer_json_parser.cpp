#include "tokenizer_json_schema.hpp"
#include "error_types.hpp"
#include "json.hpp"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

TokenizerSchema TokenizerJsonParser::parse(const std::string& json_content) {
    TokenizerSchema schema;
    
    try {
        json root = json::parse(json_content);
        
        // Parse model section
        if (!root.contains("model")) {
            throw TokenizerError("No 'model' key in tokenizer JSON");
        }
        parse_model(root["model"], schema.model);
        
        // Parse added_tokens section if it exists
        if (root.contains("added_tokens")) {
            parse_added_tokens(root["added_tokens"], schema.added_tokens);
        }
    } catch (const json::exception& e) {
        throw TokenizerError(std::string("Failed to parse tokenizer JSON: ") + e.what());
    }
    
    return schema;
}

void TokenizerJsonParser::parse_model(const nlohmann::json& model_obj,
                                     TokenizerSchema::Model& model) {
    if (!model_obj.contains("vocab")) {
        throw TokenizerError("No 'vocab' key in model");
    }
    
    const auto& vocab = model_obj["vocab"];
    
    // Load all tokens from vocab
    for (auto it = vocab.begin(); it != vocab.end(); ++it) {
        std::string token = it.key();
        int32_t id = it.value().get<int32_t>();
        model.vocab[token] = id;
    }
}

void TokenizerJsonParser::parse_added_tokens(const nlohmann::json& tokens_array,
                                            std::vector<TokenizerSchema::Token>& tokens) {
    if (!tokens_array.is_array()) {
        return;
    }
    
    for (const auto& token_obj : tokens_array) {
        if (token_obj.contains("content") && token_obj.contains("id")) {
            TokenizerSchema::Token token;
            token.content = token_obj["content"].get<std::string>();
            token.id = token_obj["id"].get<int32_t>();
            tokens.push_back(token);
        }
    }
}