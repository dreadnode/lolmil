#include "tokenizer.hpp"
#include "platform_utils.hpp"
#include "error_types.hpp"
#include "config.hpp"
#include "tokenizer_json_schema.hpp"
#include "console_output.hpp"
#include <iostream>
#include <regex>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#define NOMINMAX
#include <Windows.h>

Tokenizer::Tokenizer() {
}

Tokenizer::Tokenizer(const std::string& tokenizer_json_path) {
    load_vocabulary(tokenizer_json_path);
}

std::unique_ptr<Tokenizer> Tokenizer::FromJsonString(const std::string& json_content) {
    auto tokenizer = std::unique_ptr<Tokenizer>(new Tokenizer());
    tokenizer->load_vocabulary_from_json(json_content);
    return tokenizer;
}

void Tokenizer::load_vocabulary(const std::string& json_path) {
    std::ifstream file(json_path, std::ios::binary);
    if (!file.is_open()) {
        throw TokenizerError("Failed to open tokenizer file");
    }

    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    load_vocabulary_from_json(json_content);
}

void Tokenizer::load_vocabulary_from_json(const std::string& json_content) {
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    schema.merge_to_maps(token_to_id, id_to_token);
}

std::string Tokenizer::clean_token_string(std::string_view token) {
    std::string result(token);

    // BPE space marker: ▁ (U+2581 = 0xE2 0x96 0x81)
    size_t pos = 0;
    while ((pos = result.find(TokenizerConstants::SPACE_MARKER_UTF8, pos)) != std::string::npos) {
        result.replace(pos, TokenizerConstants::SPACE_MARKER_LEN, " ");
        pos += 1;
    }

    // Newline marker: U+010A (0xC4 0x8A)
    pos = 0;
    while ((pos = result.find(TokenizerConstants::NEWLINE_MARKER_UTF8, pos)) != std::string::npos) {
        result.replace(pos, TokenizerConstants::NEWLINE_MARKER_LEN, "\n");
        pos += 1;
    }

    pos = 0;
    while ((pos = result.find("Ċ", pos)) != std::string::npos) {
        result.replace(pos, std::string("Ċ").length(), "\n");
        pos += 1;
    }
    
    // Byte tokens format: <0xHH>
    if (result.size() >= 4 && result.substr(0, TokenizerConstants::BYTE_TOKEN_PREFIX_LEN) == TokenizerConstants::BYTE_TOKEN_PREFIX && result.back() == TokenizerConstants::BYTE_TOKEN_SUFFIX[0]) {
        std::string hex_str = result.substr(TokenizerConstants::BYTE_TOKEN_PREFIX_LEN, 
                                           result.size() - TokenizerConstants::BYTE_TOKEN_PREFIX_LEN - 1);
        try {
            int byte_val = std::stoi(hex_str, nullptr, 16);
            if (byte_val >= 0 && byte_val <= 255) {
                result = std::string(1, static_cast<char>(byte_val));
            }
        } catch (const std::exception& e) {
            ConsoleOutput::debug("Failed to convert byte token", e.what());
        }
    }
    
    return result;
}

std::vector<Tokenizer::TokenPosition> Tokenizer::split_by_special_tokens(std::string_view text) const {
    std::vector<TokenPosition> positions;
    
    const std::vector<std::pair<std::string, int32_t>> special_tokens = {
        {"<|user|>", TokenIds::USER},
        {"<|assistant|>", TokenIds::ASSISTANT},
        {"<|system|>", TokenIds::SYSTEM},
        {"<|end|>", TokenIds::END},
        {"<|endoftext|>", TokenIds::EOS},
        {"<s>", TokenIds::BOS},
        {"</s>", 2}  // EOS alternative token
    };
    
    size_t pos = 0;
    while (pos < text.size()) {
        bool found_special = false;

        for (const auto& [special_str, special_id] : special_tokens) {
            if (text.substr(pos, special_str.length()) == special_str) {
                positions.push_back({pos, pos + special_str.length(), true, special_id});
                pos += special_str.length();
                found_special = true;
                break;
            }
        }

        if (!found_special) {
            size_t next_special = text.size();
            for (const auto& [special_str, _] : special_tokens) {
                size_t found = text.find(special_str, pos);
                if (found != std::string::npos && found < next_special) {
                    next_special = found;
                }
            }
            
            if (next_special > pos) {
                positions.push_back({pos, next_special, false, -1});
                pos = next_special;
            }
        }
    }
    
    return positions;
}

void Tokenizer::handle_word_boundaries(std::string_view text, std::vector<int32_t>& tokens) const {
    std::string processed_text;
    bool at_word_start = true;
    
    for (char c : text) {
        if (c == ' ') {
            at_word_start = true;
        } else if (c == '\n') {
            if (!processed_text.empty()) {
                auto partial_tokens = tokenize_text(processed_text);
                tokens.insert(tokens.end(), partial_tokens.begin(), partial_tokens.end());
                processed_text.clear();
            }
            tokens.push_back(TokenizerConstants::NEWLINE_TOKEN_ID);
            at_word_start = true;
        } else {
            if (at_word_start && !tokens.empty()) {
                processed_text += TokenizerConstants::SPACE_MARKER_UTF8;
            }
            processed_text += c;
            at_word_start = false;
        }
    }

    if (!processed_text.empty()) {
        auto partial_tokens = tokenize_text(processed_text);
        tokens.insert(tokens.end(), partial_tokens.begin(), partial_tokens.end());
    }
}

std::vector<int32_t> Tokenizer::encode(std::string_view text, bool add_special_tokens) {
    std::scoped_lock lock(tokenizer_mutex);
    std::vector<int32_t> tokens;
    
    if (add_special_tokens) {
        auto segments = split_by_special_tokens(text);
        
        for (const auto& segment : segments) {
            if (segment.is_special) {
                if (segment.special_id != -1) {
                    tokens.push_back(segment.special_id);
                }
            } else {
                std::string regular_text(text.substr(segment.start, segment.end - segment.start));
                handle_word_boundaries(regular_text, tokens);
            }
        }
    } else {
        handle_word_boundaries(text, tokens);
    }
    
    return tokens;
}

std::string Tokenizer::format_byte_token(unsigned char byte) const {
    std::stringstream ss;
    ss << TokenizerConstants::BYTE_TOKEN_PREFIX 
       << std::uppercase << std::hex 
       << std::setw(TokenizerConstants::HEX_WIDTH) << std::setfill('0') 
       << static_cast<int>(byte) 
       << TokenizerConstants::BYTE_TOKEN_SUFFIX;
    return ss.str();
}

bool Tokenizer::try_match_byte_token(std::string_view text, size_t pos, int32_t& token_id) const {
    if (pos >= text.size()) {
        return false;
    }
    
    auto byte = static_cast<unsigned char>(text[pos]);
    std::string byte_token = format_byte_token(byte);
    
    auto it = token_to_id.find(byte_token);
    if (it != token_to_id.end()) {
        token_id = it->second;
        return true;
    }
    return false;
}

std::pair<size_t, int32_t> Tokenizer::find_longest_token_match(std::string_view text, size_t pos) const {
    size_t max_len = std::min(TokenizerConstants::MAX_TOKEN_MATCH_LENGTH, text.size() - pos);

    // Optimization: limit search to reasonable lengths, early exit for long matches
    const size_t MAX_SEARCH_LEN = 50;
    max_len = std::min(max_len, MAX_SEARCH_LEN);

    const size_t EARLY_MATCH_THRESHOLD = 10;
    
    for (size_t len = max_len; len > 0; len--) {
        std::string substr(text.substr(pos, len));

        if (len > 5) {
            bool all_same = true;
            char first_char = substr[0];
            for (size_t i = 1; i < substr.length() && i < 10; i++) {
                if (substr[i] != first_char) {
                    all_same = false;
                    break;
                }
            }
            if (all_same) {
                len = std::min(static_cast<size_t>(3), len / 4);
                continue;
            }
        }
        
        auto it = token_to_id.find(substr);
        if (it != token_to_id.end()) {
            if (len >= EARLY_MATCH_THRESHOLD) {
                return {len, it->second};
            }
            return {len, it->second};
        }
    }
    
    return {0, TokenIds::UNK};
}

std::vector<int32_t> Tokenizer::tokenize_text(std::string_view text) const {
    std::vector<int32_t> tokens;
    
    size_t pos = 0;
    while (pos < text.size()) {
        auto [match_len, token_id] = find_longest_token_match(text, pos);
        
        if (match_len > 0) {
            tokens.push_back(token_id);
            pos += match_len;
        } else {
            int32_t byte_token_id;
            if (try_match_byte_token(text, pos, byte_token_id)) {
                tokens.push_back(byte_token_id);
            } else {
                tokens.push_back(TokenIds::UNK);
            }
            pos++;
        }
    }
    
    return tokens;
}

std::string Tokenizer::decode(std::span<const int32_t> token_ids) {
    std::scoped_lock lock(tokenizer_mutex);

    const size_t MAX_TOKENS = 1000000;
    if (token_ids.size() > MAX_TOKENS) {
        throw std::invalid_argument("Token sequence too large for safe decoding");
    }

    std::string result;
    result.reserve(token_ids.size() * 4);
    
    for (int32_t id : token_ids) {
        if (id == TokenIds::BOS || id == TokenIds::EOS || id == TokenIds::PAD ||
            id == TokenIds::USER || id == TokenIds::ASSISTANT || id == TokenIds::SYSTEM || id == TokenIds::END) {
            continue;
        }
        
        auto it = id_to_token.find(id);
        if (it != id_to_token.end()) {
            std::string token = clean_token_string(it->second);

            if (result.size() + token.size() > MAX_TOKENS * 50) {
                throw std::runtime_error("Decoded text exceeds maximum size limit");
            }
            
            result += token;
        }
    }
    
    return result;
}

std::string Tokenizer::decode_token(int32_t token_id) {
    std::scoped_lock lock(tokenizer_mutex);

    auto it = id_to_token.find(token_id);
    if (it != id_to_token.end()) {
        if (token_id == TokenIds::BOS || token_id == TokenIds::EOS || token_id == TokenIds::PAD ||
            token_id == TokenIds::USER || token_id == TokenIds::ASSISTANT || 
            token_id == TokenIds::SYSTEM || token_id == TokenIds::END) {
            return "";
        }
        return clean_token_string(it->second);
    }
    return "";
}

std::string Tokenizer::format_prompt(std::string_view user_message) {
    // Phi-3 chat template
    std::string formatted = "<s><|user|>\n";
    formatted.append(user_message);
    formatted.append("<|end|>\n<|assistant|>");
    return formatted;
}