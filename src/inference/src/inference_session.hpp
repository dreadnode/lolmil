#ifndef INFERENCE_SESSION_HPP
#define INFERENCE_SESSION_HPP

#include <string>
#include <memory>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include "conversation.hpp"
#include "tokenizer.hpp"
#include "../include/inference_api.hpp"  // For GenerationResult

class InferenceSession {
private:
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Tokenizer> tokenizer;
    Conversation conversation;
    
    std::vector<Ort::Value> past_key_values;
    bool using_gpu;
    
    struct IONames {
        std::vector<const char*> input_names;
        std::vector<std::string> input_name_strings;
        std::vector<const char*> output_names;
        std::vector<std::string> output_name_strings;
    };
    IONames io_names;
    
    void initializeIONames();
    
public:
    // Made public for agent mode
    GenerationResult runInference(const std::string& formatted_prompt, float temperature, size_t max_tokens);
    InferenceSession(const std::wstring& model_path,
                    const std::string& tokenizer_path,
                    bool use_cuda = true);

    ~InferenceSession();

    [[nodiscard]] GenerationResult generate(const std::string& user_input,
                                       float temperature = 0.7f,
                                       size_t max_tokens = 200);
    
    void addSystemPrompt(const std::string& prompt);
    void addMessage(const Message& msg);
    void clearConversation();
    void removeLastMessage();
    
    [[nodiscard]] const Conversation& getConversation() const { return conversation; }
    [[nodiscard]] bool isUsingGPU() const { return using_gpu; }
};

#endif // INFERENCE_SESSION_HPP