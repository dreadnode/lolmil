#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include <string>
#include <filesystem>
#include <optional>

namespace config {

// Configuration structure for the application
struct AppConfig {
    std::filesystem::path model_path;
    std::filesystem::path tokenizer_path;
    bool use_cuda = true;
    float temperature = 0.5f;  // Balanced temperature - not too random, not too deterministic
    size_t max_tokens = 5000;  // Restored to allow complete code generation
    
    // Validate that all required paths exist
    [[nodiscard]] bool validate() const;
    
    // Get user-friendly error message if validation fails
    [[nodiscard]] std::string get_validation_error() const;
};

// Load configuration from various sources
class ConfigLoader {
public:
    // Load from JSON file
    [[nodiscard]] static std::optional<AppConfig> from_json_file(const std::filesystem::path& path);
    
    // Load from environment variables
    [[nodiscard]] static std::optional<AppConfig> from_environment();
    
    // Load from command line arguments
    [[nodiscard]] static std::optional<AppConfig> from_args(int argc, char* argv[]);
    
    // Load with fallback chain: args -> env -> config file -> defaults
    [[nodiscard]] static AppConfig load_with_fallbacks(
        int argc = 0, 
        char* argv[] = nullptr,
        const std::filesystem::path& config_file = "config.json"
    );
    
    // Get default configuration
    [[nodiscard]] static AppConfig get_defaults();
    
private:
    static void merge_config(AppConfig& target, const AppConfig& source);
};

} // namespace config

#endif // APP_CONFIG_HPP