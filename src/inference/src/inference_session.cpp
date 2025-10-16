#include "inference_session.hpp"
#include "inference_helpers.hpp"
#include "console_output.hpp"
#include "config.hpp"
#include "error_types.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

InferenceSession::InferenceSession(const std::wstring& model_path, 
                                 const std::string& tokenizer_path,
                                 bool use_cuda) 
    : using_gpu(false) {
    
    try {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "inference_session");

        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(Config::NUM_THREADS);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        if (use_cuda) {
            using_gpu = setup_cuda_provider(session_options);
        }

        session = std::make_unique<Ort::Session>(*env, model_path.c_str(), session_options);
        tokenizer = std::make_unique<Tokenizer>(tokenizer_path);
        initializeIONames();
    } catch (const Ort::Exception& e) {
        throw InferenceError(std::string("Failed to initialize ONNX session: ") + e.what());
    } catch (const std::exception& e) {
        throw InferenceError(std::string("Failed to initialize inference session: ") + e.what());
    }
}

InferenceSession::~InferenceSession() = default;

void InferenceSession::initializeIONames() {
    Ort::AllocatorWithDefaultOptions allocator;
    io_names.input_name_strings.clear();
    io_names.input_names.clear();
    io_names.output_name_strings.clear();
    io_names.output_names.clear();
    size_t num_inputs = session->GetInputCount();
    io_names.input_name_strings.reserve(num_inputs);
    for (size_t i = 0; i < num_inputs; i++) {
        auto name = session->GetInputNameAllocated(i, allocator);
        io_names.input_name_strings.emplace_back(name.get());
    }

    // ONNX Runtime API requires stable pointers - can't use temporary strings
    io_names.input_names.reserve(io_names.input_name_strings.size());
    for (const auto& name : io_names.input_name_strings) {
        io_names.input_names.push_back(name.c_str());
    }
    size_t num_outputs = session->GetOutputCount();
    io_names.output_name_strings.reserve(num_outputs);
    for (size_t i = 0; i < num_outputs; i++) {
        auto name = session->GetOutputNameAllocated(i, allocator);
        io_names.output_name_strings.emplace_back(name.get());
    }

    io_names.output_names.reserve(io_names.output_name_strings.size());
    for (const auto& name : io_names.output_name_strings) {
        io_names.output_names.push_back(name.c_str());
    }
    if (io_names.input_names.empty()) {
        throw InferenceError("No input names found in ONNX model");
    }
    if (io_names.output_names.empty()) {
        throw InferenceError("No output names found in ONNX model");
    }

    // Phi-3 specific: expects 66 inputs and 65 outputs
    if (io_names.input_names.size() != 66) {
        ConsoleOutput::print("Warning: Expected 66 inputs, got " + std::to_string(io_names.input_names.size()));
    }
    if (io_names.output_names.size() != 65) {
        ConsoleOutput::print("Warning: Expected 65 outputs, got " + std::to_string(io_names.output_names.size()));
    }
    try {
        if (io_names.input_names.size() >= 3) {
            Ort::TypeInfo type_info = session->GetInputTypeInfo(2);  // First past_key_values.0.key
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            ConsoleOutput::print("past_key_values shape: [");
            for (size_t i = 0; i < shape.size(); i++) {
                ConsoleOutput::print(std::to_string(shape[i]), false);
                if (i < shape.size() - 1) ConsoleOutput::print(", ", false);
            }
            ConsoleOutput::print("]");
        }
    } catch (const std::exception& e) {
        ConsoleOutput::print("Could not get past_key_values shape: " + std::string(e.what()));
    }
}

GenerationResult InferenceSession::generate(const std::string& user_input,
                                      float temperature,
                                      size_t max_tokens) {
    if (!user_input.empty()) {
        conversation.addMessage(Message::user(user_input));
    }

    std::string formatted_prompt = conversation.formatForModel();
    GenerationResult result = runInference(formatted_prompt, temperature, max_tokens);
    conversation.addMessage(Message::assistant(result.content));

    return result;
}

GenerationResult InferenceSession::runInference(const std::string& formatted_prompt,
                                          float temperature,
                                          size_t max_tokens) {
    GenerationResult result;
    // TODO: Implement incremental generation to reuse KV-cache across turns
    // Currently re-processing entire conversation each time
    past_key_values.clear();

    auto input_tokens = tokenizer->encode(formatted_prompt, true);
    std::vector<int64_t> full_sequence(input_tokens.begin(), input_tokens.end());
    std::vector<int64_t> attention_mask(full_sequence.size(), 1);
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::stringstream response;
    int32_t last_token = -1;
    int repetition_count = 0;
    bool first_token = true;
    size_t actual_tokens = 0;

    for (size_t i = 0; i < max_tokens; i++) {
        actual_tokens = i + 1;
        std::vector<Ort::Value> input_tensors;
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(full_sequence.size())};
        
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            mem_info, const_cast<int64_t*>(full_sequence.data()), full_sequence.size(),
            input_shape.data(), input_shape.size()));
        
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            mem_info, const_cast<int64_t*>(attention_mask.data()), attention_mask.size(),
            input_shape.data(), input_shape.size()));
        bool has_past = !past_key_values.empty();

        if (has_past) {
            for (auto& kv : past_key_values) {
                input_tensors.push_back(std::move(kv));
            }
            past_key_values.clear();
        } else {
            // Phi-3 KV-cache initialization: 32 layers × 2 (key+value) = 64 tensors
            // Shape: [batch=1, heads=32, seq_len=0, head_dim=96]
            std::vector<int64_t> empty_shape = {1, 32, 0, 96};
            Ort::Float16_t dummy_fp16(0.0f);
            
            for (size_t j = 0; j < 64; j++) {
                input_tensors.push_back(Ort::Value::CreateTensor<Ort::Float16_t>(
                    mem_info,
                    &dummy_fp16,  // Non-null pointer required by API
                    0,            // 0 elements since seq_len=0
                    empty_shape.data(),
                    empty_shape.size()
                ));
            }
        }
        if (input_tensors.size() != io_names.input_names.size()) {
            ConsoleOutput::print("\nError: Mismatch - " + std::to_string(input_tensors.size()) +
                                " tensors but " + std::to_string(io_names.input_names.size()) + " names");
            result.stop_reason = GenerationResult::StopReason::ERROR;
            break;
        }
        std::vector<Ort::Value> output_tensors;
        try {
            output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                io_names.input_names.data(),
                input_tensors.data(),
                input_tensors.size(),
                io_names.output_names.data(),
                io_names.output_names.size()
            );
        } catch (const Ort::Exception& e) {
            ConsoleOutput::print("\nONNX Runtime error: " + std::string(e.what()));
            throw InferenceError(std::string("ONNX inference failed: ") + e.what());
        }
        if (output_tensors.empty()) {
            ConsoleOutput::print("\nError: No output tensors returned!");
            result.stop_reason = GenerationResult::StopReason::ERROR;
            break;
        }
        
        auto* logits_tensor = output_tensors[0].GetTensorMutableData<Ort::Float16_t>();
        auto shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        if (shape.size() < 3) {
            ConsoleOutput::print("\nError: Unexpected logits shape!");
            result.stop_reason = GenerationResult::StopReason::ERROR;
            break;
        }
        size_t vocab_size = static_cast<size_t>(shape[2]);
        size_t last_token_idx = static_cast<size_t>(shape[1] - 1);

        // Convert FP16 logits to FP32 for token selection
        auto logits_fp32 = convert_float16_to_float32(
            logits_tensor + (last_token_idx * vocab_size), vocab_size);

        int32_t next_token = select_next_token(logits_fp32.data(), vocab_size, temperature);
        if (next_token == TokenIds::EOS || next_token == TokenIds::END) {
            result.stop_reason = GenerationResult::StopReason::END_TOKEN;
            break;
        }

        if (next_token == last_token) {
            repetition_count++;
            if (repetition_count > Config::MAX_REPETITIONS) {
                result.stop_reason = GenerationResult::StopReason::REPETITION_LIMIT;
                break;
            }
        } else {
            repetition_count = 0;
        }
        last_token = next_token;
        std::vector<int32_t> single_token = {next_token};
        std::string token_text = tokenizer->decode(single_token);

        // Debug: Log first few tokens to verify generation is starting correctly
        if (i < 10) {
            ConsoleOutput::print("Token " + std::to_string(i) + ": ID=" + std::to_string(next_token) +
                               " Text=[" + token_text + "]");
        }

        // Phi-3 quirk: often starts with extra space
        if (first_token && token_text == " ") {
            first_token = false;
            continue;
        }
        first_token = false;

        response << token_text;

        // Early exit when code block is complete to avoid hallucinated explanations
        std::string current_response = response.str();
        size_t code_start = current_response.find("```lua");
        if (code_start != std::string::npos) {
            size_t search_from = code_start + 6;
            size_t code_end = current_response.find("```", search_from);

            if (code_end != std::string::npos) {
                std::string code_content = current_response.substr(search_from, code_end - search_from);
                bool has_content = false;
                for (char c : code_content) {
                    if (!std::isspace(c)) {
                        has_content = true;
                        break;
                    }
                }

                if (has_content) {
                    size_t after_code = code_end + 3;
                    if (after_code < current_response.length() - 1) {  // Allow trailing newline
                        response.str("");
                        response << current_response.substr(0, after_code);
                        result.stop_reason = GenerationResult::StopReason::CODE_BLOCK_COMPLETE;
                        break;
                    }
                }
            }
        }

        // Stop on common hallucination patterns
        if (current_response.find("\n- Assistant:") != std::string::npos ||
            current_response.find("\n\n\n") != std::string::npos ||
            current_response.find("**Instruction") != std::string::npos) {
            size_t stop_pos = current_response.find("\n- Assistant:");
            if (stop_pos == std::string::npos) stop_pos = current_response.find("\n\n\n");
            if (stop_pos == std::string::npos) stop_pos = current_response.find("**Instruction");
            if (stop_pos != std::string::npos) {
                response.str("");
                response << current_response.substr(0, stop_pos);
                result.stop_reason = GenerationResult::StopReason::STOP_PATTERN;
                break;
            }
        }
        full_sequence.push_back(static_cast<int64_t>(next_token));
        attention_mask.push_back(1);

        past_key_values.clear();
        if (output_tensors.size() == 65) {  // 1 logits + 64 past_key_values
            for (size_t j = 1; j < output_tensors.size(); j++) {
                past_key_values.push_back(std::move(output_tensors[j]));
            }
        } else {
            ConsoleOutput::print("\nWarning: Unexpected number of outputs: " + std::to_string(output_tensors.size()));
        }
    }
    if (actual_tokens >= max_tokens) {
        result.truncated = true;
        result.stop_reason = GenerationResult::StopReason::MAX_TOKENS;
    }
    result.content = response.str();
    result.tokens_used = actual_tokens;

    return result;
}

void InferenceSession::addSystemPrompt(const std::string& prompt) {
    conversation.setSystemPrompt(prompt);
}

void InferenceSession::addMessage(const Message& msg) {
    conversation.addMessage(msg);
}

void InferenceSession::clearConversation() {
    conversation.clear();
    past_key_values.clear();
}

void InferenceSession::removeLastMessage() {
    conversation.removeLastMessage();
    // KV-cache intentionally not cleared - full regeneration too expensive
    // Minor context inconsistency acceptable for retry scenarios
}