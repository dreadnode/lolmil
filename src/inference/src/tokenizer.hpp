#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <mutex>
#include <memory>
#include "config.hpp"

class Tokenizer {
private:
    std::unordered_map<std::string, int32_t> token_to_id;
    std::unordered_map<int32_t, std::string> id_to_token;
    mutable std::mutex tokenizer_mutex;
    
public:
 // Constructor that loads from file
 explicit Tokenizer(const std::string& tokenizer_json_path);
 
 // Factory method that uses JSON content directly (for testing)
 static std::unique_ptr<Tokenizer> FromJsonString(const std::string& json_content);
 
 // Delete copy operations (mutex is not copyable)
 Tokenizer(const Tokenizer&) = delete;
 Tokenizer& operator=(const Tokenizer&) = delete;
 
 // Delete move operations (mutex is not movable)
 Tokenizer(Tokenizer&&) = delete;
 Tokenizer& operator=(Tokenizer&&) = delete;

 // Encode text to token IDs
 [[nodiscard]] std::vector<int32_t> encode(std::string_view text,
                                           bool add_special_tokens = true);

 // Decode token IDs back to text (C++20 std::span for flexibility)
 [[nodiscard]] std::string decode(std::span<const int32_t> token_ids);

 // Decode a single token
 [[nodiscard]] std::string decode_token(int32_t token_id);

 // Format a prompt for chat (Phi-3 format)
 [[nodiscard]] std::string format_prompt(std::string_view user_message);

 // Get special token IDs
 [[nodiscard]] constexpr int32_t get_bos_token_id() const noexcept { return TokenIds::BOS; }
 [[nodiscard]] constexpr int32_t get_eos_token_id() const noexcept { return TokenIds::EOS; }
 [[nodiscard]] constexpr int32_t get_pad_token_id() const noexcept { return TokenIds::PAD; }

 // Check if a token is an end token
 // According to genai_config.json: eos_token_id: [32000, 32001, 32007]
 // But 32001 is <|assistant|> which should NOT stop generation
 [[nodiscard]] constexpr bool is_end_token(int32_t token_id) const noexcept {
   return token_id == TokenIds::EOS ||
          token_id == TokenIds::END;  // Only <|endoftext|> and <|end|>
    }
    
private:
    // Private constructor for direct JSON loading
    Tokenizer();
    
    void load_vocabulary(const std::string& json_path);
    void load_vocabulary_from_json(const std::string& json_content);
    [[nodiscard]] std::string clean_token_string(std::string_view token);
    [[nodiscard]] std::vector<int32_t> tokenize_text(std::string_view text) const;
    
    // Helper methods for encode
    struct TokenPosition {
        size_t start;
        size_t end;
        bool is_special;
        int32_t special_id;
    };
    
    [[nodiscard]] std::vector<TokenPosition> split_by_special_tokens(std::string_view text) const;
    [[nodiscard]] std::vector<int32_t> process_regular_text(std::string_view text) const;
    void handle_word_boundaries(std::string_view text, std::vector<int32_t>& tokens) const;
    
    // Token processing helpers
    [[nodiscard]] std::string format_byte_token(unsigned char byte) const;
    [[nodiscard]] bool try_match_byte_token(std::string_view text, size_t pos, int32_t& token_id) const;
    [[nodiscard]] std::pair<size_t, int32_t> find_longest_token_match(std::string_view text, size_t pos) const;
};

#endif // TOKENIZER_HPP