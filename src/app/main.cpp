#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include "inference_api.hpp"
#include "../inference/src/inference_session.hpp"
#include "app_config.hpp"
#include "lua_api.hpp"
#include "lua_tools.hpp"
#include "win32_tools.hpp"
#include "filesystem_tools.hpp"
#include "console_output.hpp"
#include "error_types.hpp"
#include "minimal_prompt.hpp"

namespace fs = std::filesystem;

struct ConversationLog {
    struct Exchange {
        std::string prompt;
        std::string generated_code;
        std::string execution_result;
    };

    std::string system_prompt;
    std::vector<Exchange> exchanges;

    void addExchange(const std::string& prompt, const std::string& generated, const std::string& result) {
        exchanges.push_back({prompt, generated, result});
    }

    void saveToFile() const {
        fs::path logs_dir = "logs";
        if (!fs::exists(logs_dir)) {
            fs::create_directory(logs_dir);
        }

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        std::ostringstream filename;
        filename << "conversation_log_" 
                 << std::put_time(&tm, "%Y%m%d_%H%M%S") 
                 << ".txt";
        
        fs::path log_path = logs_dir / filename.str();
        std::ofstream log_file(log_path);
        if (!log_file) {
            ConsoleOutput::error("Failed to create log file: " + log_path.string());
            return;
        }

        log_file << "[SYSTEM]\n" << system_prompt << "\n\n";

        for (const auto& exchange : exchanges) {
            log_file << "[USER]\n" << exchange.prompt << "\n\n";
            log_file << "[ASSISTANT]\n" << exchange.generated_code << "\n\n";
            if (!exchange.execution_result.empty()) {
                log_file << "[EXECUTION RESULT]\n" << exchange.execution_result << "\n\n";
            }
            log_file << "---\n\n";
        }
        
        log_file.close();
        ConsoleOutput::print(L"Conversation saved to: " + log_path.wstring());
    }
};

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    try {
        std::string single_prompt;
        bool single_prompt_mode = false;
        bool agent_mode = false;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if ((arg == "--prompt" || arg == "-p") && i + 1 < argc) {
                single_prompt = argv[++i];
                single_prompt_mode = true;
            } else if (arg == "--agent" || arg == "-a") {
                agent_mode = true;
            }
        }

        ConsoleOutput::print(L"Initializing Phi-3 Mini with ONNX Runtime...");

        auto app_config = config::ConfigLoader::load_with_fallbacks(argc, argv);

        if (!app_config.validate()) {
            ConsoleOutput::error("Configuration error:\n" + app_config.get_validation_error());
            ConsoleOutput::print(L"\nUsage: " + std::wstring(argv[0] ? fs::path(argv[0]).filename().wstring() : L"onnxtest") + L" [options]");
            ConsoleOutput::print(L"Options:");
            ConsoleOutput::print(L"  --model, -m <path>      Path to ONNX model file");
            ConsoleOutput::print(L"  --tokenizer, -t <path>  Path to tokenizer JSON file");
            ConsoleOutput::print(L"  --prompt, -p <text>     Run single prompt and exit");
            ConsoleOutput::print(L"  --no-cuda               Disable CUDA acceleration");
            ConsoleOutput::print(L"  --temperature <float>   Set temperature (0.0-2.0)");
            ConsoleOutput::print(L"  --max-tokens <int>      Set max tokens (1-10000)");
            ConsoleOutput::print(L"\nEnvironment variables:");
            ConsoleOutput::print(L"  PHI3_MODEL_PATH         Path to model file");
            ConsoleOutput::print(L"  PHI3_TOKENIZER_PATH     Path to tokenizer file");
            ConsoleOutput::print(L"  PHI3_USE_CUDA           Enable CUDA (1/true)");
            return 1;
        }
        
        const std::wstring model_path = app_config.model_path.wstring();
        const std::string tokenizer_path = app_config.tokenizer_path.string();

        ConsoleOutput::print(L"Loading model from: " + app_config.model_path.wstring());
        InferenceSession session(model_path, tokenizer_path, app_config.use_cuda);
        
        ConsoleOutput::print(std::wstring(L"Ready! Using ") + 
                           (session.isUsingGPU() ? L"GPU" : L"CPU") + 
                           L" acceleration.");

        ConsoleOutput::print(L"Initializing Lua interpreter...");
        lua::LuaRuntime lua_runtime;

        lua::tools::ToolRegistry tool_registry;
        lua::win32::registerTools(tool_registry, lua_runtime.getState());
        lua::win32::registerAgentControl(tool_registry, lua_runtime.getState());
        lua::filesystem::registerTools(tool_registry, lua_runtime.getState());

        std::string system_prompt = prompts::MINIMAL_PROMPT;

        session.addSystemPrompt(system_prompt);
        ConversationLog conversation_log;
        conversation_log.system_prompt = system_prompt;

        if (agent_mode) {
            ConsoleOutput::print(L"\n=== Agent Mode ===");
            if (single_prompt_mode) {
                ConsoleOutput::print(L"Initial prompt: " + std::wstring(single_prompt.begin(), single_prompt.end()));
            } else {
                ConsoleOutput::print(L"Enter initial prompt for agent:");
            }
        } else if (single_prompt_mode) {
            ConsoleOutput::print(L"\n=== Single Prompt Mode ===");
            ConsoleOutput::print(L"Prompt: " + std::wstring(single_prompt.begin(), single_prompt.end()));
        } else {
            ConsoleOutput::print(L"\n=== Interactive Lua Session ===");
        }
        
        bool continue_conversation = true;
        while (continue_conversation) {
            std::string prompt;
            bool is_agent_iteration = false;
            
            if (agent_mode) {
                static bool first_agent_iteration = true;
                static int agent_iterations = 0;
                const int MAX_AGENT_ITERATIONS = 10;
                
                if (first_agent_iteration) {
                    if (single_prompt_mode) {
                        prompt = single_prompt;
                    } else {
                        ConsoleOutput::print(L"\n> ", false);
                        prompt = ConsoleOutput::getInput();
                        if (std::cin.eof() || std::cin.fail() || prompt == "exit" || prompt == "quit") {
                            conversation_log.saveToFile();
                            break;
                        }
                    }
                    first_agent_iteration = false;
                    agent_iterations = 0;
                    lua::win32::resetAgentState();
                } else {
                    is_agent_iteration = true;
                    agent_iterations++;

                    if (lua::win32::isAgentComplete()) {
                        ConsoleOutput::print(L"\n[AGENT TASK COMPLETE]");
                        ConsoleOutput::print(std::wstring(lua::win32::getAgentMessage().begin(), 
                                                        lua::win32::getAgentMessage().end()));
                        conversation_log.saveToFile();
                        break;
                    }
                    
                    if (agent_iterations >= MAX_AGENT_ITERATIONS) {
                        ConsoleOutput::print(L"\n[AGENT TIMEOUT] Reached maximum iterations");
                        conversation_log.saveToFile();
                        break;
                    }
                }
            } else if (single_prompt_mode) {
                prompt = single_prompt;
                continue_conversation = false;
            } else {
                ConsoleOutput::print(L"\n> ", false);
                prompt = ConsoleOutput::getInput();

                if (std::cin.eof() || std::cin.fail()) {
                    ConsoleOutput::print(L"\nInput stream closed. Exiting...");
                    conversation_log.saveToFile();
                    break;
                }
                
                if (prompt == "exit" || prompt == "quit") {
                    conversation_log.saveToFile();
                    break;
                }
                
                if (prompt == "reset") {
                    lua_runtime.reset();
                    tool_registry.clear();
                    lua::win32::registerTools(tool_registry, lua_runtime.getState());
                    lua::win32::registerAgentControl(tool_registry, lua_runtime.getState());
                    lua::filesystem::registerTools(tool_registry, lua_runtime.getState());
                    session.clearConversation();
                    ConsoleOutput::print(L"Conversation and Lua state reset.");
                    continue;
                }
                
                if (prompt.empty()) {
                    continue;
                }
            }

            GenerationResult response;
            if (!is_agent_iteration) {
                ConsoleOutput::print(L"\n[GENERATING CODE]");
                try {
                    response = session.generate(prompt, app_config.temperature, app_config.max_tokens);
                } catch (const std::exception& e) {
                    ConsoleOutput::error(std::string("Generation failed: ") + e.what());
                    continue;
                }
            } else {
                ConsoleOutput::print(L"\n[AGENT CONTINUING...]");
                try {
                    // Generate without adding a new user message (tool message already added)
                    // The session already has the conversation with the tool message
                    // We just need to generate the next response
                    response = session.generate("", app_config.temperature, app_config.max_tokens);
                } catch (const std::exception& e) {
                    ConsoleOutput::error(std::string("Agent generation failed: ") + e.what());
                    continue;
                }
            }

            ConsoleOutput::print(L"\n[GENERATED CODE]");
            ConsoleOutput::print(L"Response length: " + std::to_wstring(response.content.length()));
            if (response.truncated) {
                ConsoleOutput::print(L"[WARNING] Generation truncated at token limit (" + std::to_wstring(response.tokens_used) + L" tokens)");
            }
            if (response.content.length() < 50) {
                // Show the raw response for debugging if it's short
                ConsoleOutput::print(L"Raw response: [" + std::wstring(response.content.begin(), response.content.end()) + L"]");
            }
            ConsoleOutput::print(response.content);

            std::string execution_result;

            // Log truncation warning if needed
            if (response.truncated) {
                execution_result = "[TOKEN_LIMIT_REACHED at " + std::to_string(response.tokens_used) + " tokens]\n";
            }

            auto code_blocks = CodeExtractor::extractFromLastResponse(session.getConversation());
            auto lua_blocks = CodeExtractor::filterByLanguage(code_blocks, "lua");

            bool syntax_valid = true;
            if (!lua_blocks.empty()) {
                for (const auto& block : lua_blocks) {
                    auto syntaxCheck = lua_runtime.checkSyntax(block.code);
                    if (!syntaxCheck.isOk()) {
                        syntax_valid = false;
                        const auto& error = syntaxCheck.error();
                        if (std::holds_alternative<lua::SyntaxError>(error)) {
                            const auto& syntax_error = std::get<lua::SyntaxError>(error);
                            ConsoleOutput::print(L"\n[SYNTAX ERROR IN EXTRACTED CODE]");
                            ConsoleOutput::error(syntax_error.what());
                            execution_result += "[REJECTED - SYNTAX ERROR] " + syntax_error.what() + "\n";
                        }
                        break;
                    }
                }
                if (!syntax_valid) {
                    lua_blocks.clear();
                }
            }

            const int MAX_RETRIES = 10;
            int retry_count = 0;

            while (lua_blocks.empty() && retry_count < MAX_RETRIES) {
                retry_count++;
                ConsoleOutput::print(L"\n[NO VALID CODE FOUND - RETRY " + std::to_wstring(retry_count) + L"/" + std::to_wstring(MAX_RETRIES) + L"]");

                if (retry_count == 1) {
                    execution_result += "[REJECTED - INCOMPLETE CODE BLOCK]\n";
                    execution_result += "Retry attempt " + std::to_string(retry_count) + "/" + std::to_string(MAX_RETRIES) + "\n";
                }

                // Note: We check the last message in the history to see if it's an assistant message
                const auto& history = session.getConversation().getHistory();
                if (!history.empty() && history.back().role == Message::Assistant) {
                    session.removeLastMessage();
                }

                session.addMessage(Message::system("Your previous response did not contain a valid Lua code block. Please generate ONLY Lua code between ```lua and ``` markers. No explanations or other text."));

                try {
                    response = session.generate("", app_config.temperature, app_config.max_tokens);
                    ConsoleOutput::print(L"\n[RETRY GENERATED]");
                    ConsoleOutput::print(L"Response length: " + std::to_wstring(response.content.length()));
                    ConsoleOutput::print(response.content);

                    code_blocks = CodeExtractor::extractFromLastResponse(session.getConversation());
                    lua_blocks = CodeExtractor::filterByLanguage(code_blocks, "lua");

                    if (!lua_blocks.empty()) {
                        bool retry_syntax_valid = true;
                        for (const auto& block : lua_blocks) {
                            auto syntaxCheck = lua_runtime.checkSyntax(block.code);
                            if (!syntaxCheck.isOk()) {
                                retry_syntax_valid = false;
                                const auto& error = syntaxCheck.error();
                                if (std::holds_alternative<lua::SyntaxError>(error)) {
                                    const auto& syntax_error = std::get<lua::SyntaxError>(error);
                                    ConsoleOutput::print(L"\n[RETRY SYNTAX ERROR]");
                                    ConsoleOutput::error(syntax_error.what());
                                    execution_result += "Retry " + std::to_string(retry_count) + " failed - syntax error: " + syntax_error.what() + "\n";
                                }
                                break;
                            }
                        }
                        if (!retry_syntax_valid) {
                            lua_blocks.clear();
                        }
                    }

                    if (lua_blocks.empty() && retry_count < MAX_RETRIES) {
                        if (execution_result.find("Retry " + std::to_string(retry_count) + " failed") == std::string::npos) {
                            execution_result += "Retry " + std::to_string(retry_count) + " failed - no valid code block\n";
                        }
                    }
                } catch (const std::exception& e) {
                    ConsoleOutput::error(std::string("Retry generation failed: ") + e.what());
                    execution_result += "Retry " + std::to_string(retry_count) + " failed with error: " + e.what() + "\n";
                    break;
                }
            }
            
            if (!lua_blocks.empty()) {
                ConsoleOutput::print(L"\n[EXECUTING]");

                if (retry_count > 0) {
                    execution_result += "Retry " + std::to_string(retry_count) + " successful - valid code block found\n";
                }

                for (const auto& block : lua_blocks) {
                    ConsoleOutput::print(L"\n[DEBUG] Code length: " + std::to_wstring(block.code.length()));
                    std::wstring code_preview;
                    if (block.code.length() <= 100) {
                        code_preview = std::wstring(block.code.begin(), block.code.end());
                    } else {
                        std::string first = block.code.substr(0, 50);
                        std::string last = block.code.substr(block.code.length() - 50);
                        code_preview = std::wstring(first.begin(), first.end()) + L"..." + std::wstring(last.begin(), last.end());
                    }
                    ConsoleOutput::print(L"[DEBUG] Code content: " + code_preview);

                    ConsoleOutput::print(L"\n[RESULT]");

                    auto result = lua_runtime.execute(block.code);
                    
                    if (result.isOk()) {
                        const auto& output = result.value();
                        if (!output.printed_output.empty()) {
                            ConsoleOutput::print(output.printed_output);
                            execution_result = output.printed_output;
                        } else {
                            ConsoleOutput::print(L"(No output)");
                            execution_result = "(No output)";
                        }
                        
                        if (agent_mode) {
                            // In agent mode, use Tool messages for execution results
                            std::string tool_result = "[Lua execution result]\n";
                            if (!output.printed_output.empty()) {
                                tool_result += output.printed_output;
                            } else {
                                tool_result += "(Code executed successfully with no output)";
                            }
                            tool_result += "\n\n[CONTINUE] You are still in the agent loop. Continue working on the task. Submit more code or call win32.EndAgent() when complete.";
                            session.addMessage(Message::tool(tool_result));

                            // Clear Lua runtime for next iteration to avoid variable pollution
                            lua_runtime.reset();
                            tool_registry.clear();
                            lua::win32::registerTools(tool_registry, lua_runtime.getState());
                            lua::win32::registerAgentControl(tool_registry, lua_runtime.getState());
                            lua::filesystem::registerTools(tool_registry, lua_runtime.getState());
                        } else {
                            std::string feedback = "The Lua code executed successfully.";
                            if (!output.printed_output.empty()) {
                                feedback += " Output: " + output.printed_output;
                            }
                            session.addMessage(Message::system(feedback));
                        }
                        
                    } else {
                        ConsoleOutput::print(L"[ERROR]");
                        std::string error_msg = std::visit([](const auto& err) {
                            return err.what();
                        }, result.error());
                        ConsoleOutput::print(error_msg);
                        execution_result = "[ERROR] " + error_msg;

                        if (agent_mode) {
                            std::string tool_error = "[Lua execution error]\n" + error_msg;
                            tool_error += "\n\n[CONTINUE] Error encountered. You are still in the agent loop. Try a different approach or call win32.EndAgent() if the task cannot be completed.";
                            session.addMessage(Message::tool(tool_error));

                            lua_runtime.reset();
                            tool_registry.clear();
                            lua::win32::registerTools(tool_registry, lua_runtime.getState());
                            lua::win32::registerAgentControl(tool_registry, lua_runtime.getState());
                            lua::filesystem::registerTools(tool_registry, lua_runtime.getState());
                        } else {
                            std::string error_feedback = "ERROR in generated code:\n" + error_msg + 
                                "\n\nFIX: Always check if functions return nil:\n" +
                                "local info, err = win32.GetService(name)\n" +
                                "if info and info.binaryPath then\n" +
                                "  local sec, err = win32.GetFileSecurity(info.binaryPath)\n" +
                                "  if sec and sec.writable_by_everyone then\n" +
                                "    print('Found vulnerable service')\n" +
                                "  end\n" +
                                "end";
                            session.addMessage(Message::system(error_feedback));
                        }
                    }
                }
            } else {
                if (retry_count >= MAX_RETRIES) {
                    ConsoleOutput::error(L"[RETRIES EXHAUSTED] Failed to generate valid code after " + std::to_wstring(MAX_RETRIES) + L" attempts");
                    execution_result = "[RETRIES EXHAUSTED]";
                } else {
                    ConsoleOutput::print(L"\n[NO CODE FOUND]");
                    execution_result = "[NO CODE FOUND]";
                }
                if (agent_mode) {
                    session.addMessage(Message::tool("[NO CODE FOUND]\nYour response did not contain valid Lua code.\n\n[CONTINUE] You are still in the agent loop. Generate Lua code between ```lua and ``` markers to continue working on the task, or call win32.EndAgent() if complete."));
                } else {
                    session.addMessage(Message::system("Failed to generate valid Lua code. Please ensure future responses use ```lua at the start and ``` at the end of code blocks."));
                }
            }

            conversation_log.addExchange(prompt, response.content, execution_result);
        }

        if (single_prompt_mode) {
            conversation_log.saveToFile();
        }
        
    } catch (const Ort::Exception& e) {
        ConsoleOutput::error(std::string("ONNX Runtime error: ") + e.what());
        return 1;
    } catch (const TokenizerError& e) {
        ConsoleOutput::error(std::string("Tokenizer error: ") + e.what());
        return 1;
    } catch (const ModelError& e) {
        ConsoleOutput::error(std::string("Model error: ") + e.what());
        return 1;
    } catch (const InferenceError& e) {
        ConsoleOutput::error(std::string("Inference error: ") + e.what());
        return 1;
    } catch (const std::exception& e) {
        ConsoleOutput::error(std::string("Error: ") + e.what());
        return 1;
    }
    
    return 0;
}