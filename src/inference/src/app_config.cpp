#include "app_config.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace config {

bool AppConfig::validate() const {
    if (!fs::exists(model_path)) {
        return false;
    }
    if (!fs::exists(tokenizer_path)) {
        return false;
    }
    if (temperature <= 0.0f || temperature > 2.0f) {
        return false;
    }
    if (max_tokens == 0 || max_tokens > 10000) {
        return false;
    }
    return true;
}

std::string AppConfig::get_validation_error() const {
    std::stringstream ss;
    
    if (!fs::exists(model_path)) {
        ss << "Model file not found: " << model_path << "\n";
    }
    if (!fs::exists(tokenizer_path)) {
        ss << "Tokenizer file not found: " << tokenizer_path << "\n";
    }
    if (temperature <= 0.0f || temperature > 2.0f) {
        ss << "Temperature must be between 0 and 2, got: " << temperature << "\n";
    }
    if (max_tokens == 0 || max_tokens > 10000) {
        ss << "Max tokens must be between 1 and 10000, got: " << max_tokens << "\n";
    }
    
    return ss.str();
}

std::optional<AppConfig> ConfigLoader::from_json_file(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        json j;
        file >> j;
        
        AppConfig config;
        
        if (j.contains("model_path")) {
            config.model_path = j["model_path"].get<std::string>();
        }
        if (j.contains("tokenizer_path")) {
            config.tokenizer_path = j["tokenizer_path"].get<std::string>();
        }
        if (j.contains("use_cuda")) {
            config.use_cuda = j["use_cuda"].get<bool>();
        }
        if (j.contains("temperature")) {
            config.temperature = j["temperature"].get<float>();
        }
        if (j.contains("max_tokens")) {
            config.max_tokens = j["max_tokens"].get<size_t>();
        }
        
        return config;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config file: " << e.what() << "\n";
        return std::nullopt;
    }
}

std::optional<AppConfig> ConfigLoader::from_environment() {
    AppConfig config;
    bool has_config = false;
    
    if (const char* model = std::getenv("PHI3_MODEL_PATH")) {
        config.model_path = model;
        has_config = true;
    }
    
    if (const char* tokenizer = std::getenv("PHI3_TOKENIZER_PATH")) {
        config.tokenizer_path = tokenizer;
        has_config = true;
    }
    
    if (const char* cuda = std::getenv("PHI3_USE_CUDA")) {
        config.use_cuda = (std::string(cuda) == "1" || 
                          std::string(cuda) == "true" || 
                          std::string(cuda) == "TRUE");
        has_config = true;
    }
    
    if (const char* temp = std::getenv("PHI3_TEMPERATURE")) {
        try {
            config.temperature = std::stof(temp);
            has_config = true;
        } catch (...) {
            // Invalid temperature, ignore
        }
    }
    
    if (const char* tokens = std::getenv("PHI3_MAX_TOKENS")) {
        try {
            config.max_tokens = std::stoul(tokens);
            has_config = true;
        } catch (...) {
            // Invalid max_tokens, ignore
        }
    }
    
    return has_config ? std::optional<AppConfig>(config) : std::nullopt;
}

std::optional<AppConfig> ConfigLoader::from_args(int argc, char* argv[]) {
    if (argc < 2) {
        return std::nullopt;
    }
    
    AppConfig config;
    bool has_config = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if ((arg == "--model" || arg == "-m") && i + 1 < argc) {
            config.model_path = argv[++i];
            has_config = true;
        } else if ((arg == "--tokenizer" || arg == "-t") && i + 1 < argc) {
            config.tokenizer_path = argv[++i];
            has_config = true;
        } else if (arg == "--no-cuda") {
            config.use_cuda = false;
            has_config = true;
        } else if (arg == "--temperature" && i + 1 < argc) {
            try {
                config.temperature = std::stof(argv[++i]);
                has_config = true;
            } catch (...) {
                // Invalid temperature, ignore
            }
        } else if (arg == "--max-tokens" && i + 1 < argc) {
            try {
                config.max_tokens = std::stoul(argv[++i]);
                has_config = true;
            } catch (...) {
                // Invalid max_tokens, ignore
            }
        }
    }
    
    return has_config ? std::optional<AppConfig>(config) : std::nullopt;
}

AppConfig ConfigLoader::load_with_fallbacks(
    int argc, 
    char* argv[],
    const fs::path& config_file
) {
    // Start with defaults
    AppConfig config = get_defaults();
    
    // Try to load from config file
    if (auto file_config = from_json_file(config_file)) {
        merge_config(config, *file_config);
    }
    
    // Override with environment variables
    if (auto env_config = from_environment()) {
        merge_config(config, *env_config);
    }
    
    // Override with command line arguments
    if (argc > 0 && argv != nullptr) {
        if (auto args_config = from_args(argc, argv)) {
            merge_config(config, *args_config);
        }
    }
    
    return config;
}

AppConfig ConfigLoader::get_defaults() {
    AppConfig config;
    
    // Default paths - these should be updated based on your system
    const fs::path default_model_dir = "C:/Users/mharley/Downloads/phi-3-mini-cuda-fp16";
    config.model_path = default_model_dir / "phi3-mini-4k-instruct-cuda-fp16.onnx";
    config.tokenizer_path = default_model_dir / "tokenizer.json";
    
    // Check if defaults exist, if not try common locations
    if (!fs::exists(config.model_path)) {
        // Try current directory
        if (fs::exists("phi3-mini-4k-instruct-cuda-fp16.onnx")) {
            config.model_path = "phi3-mini-4k-instruct-cuda-fp16.onnx";
        }
        // Try models subdirectory
        else if (fs::exists("models/phi3-mini-4k-instruct-cuda-fp16.onnx")) {
            config.model_path = "models/phi3-mini-4k-instruct-cuda-fp16.onnx";
        }
    }
    
    if (!fs::exists(config.tokenizer_path)) {
        // Try current directory
        if (fs::exists("tokenizer.json")) {
            config.tokenizer_path = "tokenizer.json";
        }
        // Try models subdirectory
        else if (fs::exists("models/tokenizer.json")) {
            config.tokenizer_path = "models/tokenizer.json";
        }
    }
    
    config.use_cuda = true;
    config.temperature = 0.7f;
    config.max_tokens = 5000;
    
    return config;
}

void ConfigLoader::merge_config(AppConfig& target, const AppConfig& source) {
    if (!source.model_path.empty()) {
        target.model_path = source.model_path;
    }
    if (!source.tokenizer_path.empty()) {
        target.tokenizer_path = source.tokenizer_path;
    }
    // For bool and numeric values, always override (can't check for "unset")
    target.use_cuda = source.use_cuda;
    target.temperature = source.temperature;
    target.max_tokens = source.max_tokens;
}

} // namespace config