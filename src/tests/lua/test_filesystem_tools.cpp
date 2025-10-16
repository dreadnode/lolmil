#include <gtest/gtest.h>
#include "lua_api.hpp"
#include "lua_tools.hpp"
#include "filesystem_tools.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class FileSystemToolsTest : public ::testing::Test {
protected:
    lua::LuaRuntime runtime;
    lua::tools::ToolRegistry registry;
    
    void SetUp() override {
        // Register filesystem tools
        lua::filesystem::registerTools(registry, runtime.getState());
        
        // Create test directory structure
        fs::create_directories("test_fs/subdir");
        std::ofstream("test_fs/file1.txt") << "Content 1";
        std::ofstream("test_fs/file2.txt") << "Content 2";
        std::ofstream("test_fs/subdir/file3.txt") << "Content 3";
    }
    
    void TearDown() override {
        // Clean up test files
        fs::remove_all("test_fs");
        fs::remove("test_copy.txt");
        fs::remove("test_move.txt");
    }
};

// Test Lua integration
TEST_F(FileSystemToolsTest, LuaIntegration_PathOperations) {
    auto result = runtime.execute(R"(
        -- Test join
        local path = fs.join('folder', 'subfolder', 'file.txt')
        if not path:find('folder') then return false end
        if not path:find('file.txt') then return false end
        
        -- Test dirname and basename
        local dir = fs.dirname('/path/to/file.txt')
        local base = fs.basename('/path/to/file.txt')
        if base ~= 'file.txt' then return false end
        
        -- Test extension
        local ext = fs.extension('document.pdf')
        if ext ~= '.pdf' then return false end
        
        return true
    )");
    
    ASSERT_TRUE(result.isOk());
}

TEST_F(FileSystemToolsTest, LuaIntegration_FileOperations) {
    auto result = runtime.execute(R"(
        -- Create and read file
        if not fs.writeText('test_fs/lua_test.txt', 'Lua content') then
            return false
        end
        
        local content = fs.readText('test_fs/lua_test.txt')
        if content ~= 'Lua content' then return false end
        
        -- Check file exists and size
        if not fs.exists('test_fs/lua_test.txt') then return false end
        if not fs.isFile('test_fs/lua_test.txt') then return false end
        
        local size = fs.fileSize('test_fs/lua_test.txt')
        if size ~= 11 then return false end  -- "Lua content"
        
        -- Remove file
        if not fs.remove('test_fs/lua_test.txt') then return false end
        if fs.exists('test_fs/lua_test.txt') then return false end
        
        return true
    )");
    
    ASSERT_TRUE(result.isOk());
}

TEST_F(FileSystemToolsTest, LuaIntegration_DirectoryOperations) {
    auto result = runtime.execute(R"(
        -- Create directory
        if not fs.createDirs('test_fs/lua/nested/dirs') then
            return false
        end
        
        -- Check it exists
        if not fs.exists('test_fs/lua/nested/dirs') then return false end
        if not fs.isDirectory('test_fs/lua/nested/dirs') then return false end
        
        -- List directory
        local files = fs.listDir('test_fs')
        if type(files) ~= 'table' then return false end
        
        local found_subdir = false
        for _, name in ipairs(files) do
            if name == 'subdir' then
                found_subdir = true
            end
        end
        if not found_subdir then return false end
        
        -- Get current and temp dirs
        local current = fs.currentDir()
        local temp = fs.tempDir()
        if type(current) ~= 'string' or #current == 0 then return false end
        if type(temp) ~= 'string' or #temp == 0 then return false end
        
        return true
    )");
    
    ASSERT_TRUE(result.isOk());
}

TEST_F(FileSystemToolsTest, LuaIntegration_CopyMove) {
    auto result = runtime.execute(R"(
        -- Write original file
        if not fs.writeText('test_fs/original.txt', 'Original content') then
            return false, "Failed to write original"
        end
        
        -- Copy file
        if not fs.copy('test_fs/original.txt', 'test_fs/copy.txt') then
            return false, "Failed to copy"
        end
        
        -- Verify copy
        local copy_content = fs.readText('test_fs/copy.txt')
        if copy_content ~= 'Original content' then
            return false, "Copy has wrong content"
        end
        
        -- Move file
        if not fs.move('test_fs/copy.txt', 'test_fs/moved.txt') then
            return false, "Failed to move"
        end
        
        -- Verify move
        if fs.exists('test_fs/copy.txt') then
            return false, "Original file still exists after move"
        end
        if not fs.exists('test_fs/moved.txt') then
            return false, "Moved file doesn't exist"
        end
        
        return true
    )");
    
    ASSERT_TRUE(result.isOk());
}

// Test system prompt generation
TEST_F(FileSystemToolsTest, SystemPromptGeneration) {
    auto prompt = registry.getSystemPrompt();
    
    // Check that prompt contains expected sections
    EXPECT_TRUE(prompt.find("## fs Module") != std::string::npos);
    
    // Check for specific functions
    EXPECT_TRUE(prompt.find("fs.join") != std::string::npos);
    EXPECT_TRUE(prompt.find("fs.readText") != std::string::npos);
    EXPECT_TRUE(prompt.find("fs.writeText") != std::string::npos);
    EXPECT_TRUE(prompt.find("fs.exists") != std::string::npos);
    EXPECT_TRUE(prompt.find("fs.createDirs") != std::string::npos);
    
    // Check for documentation
    EXPECT_TRUE(prompt.find("Join path components") != std::string::npos);
    EXPECT_TRUE(prompt.find("Read text file contents") != std::string::npos);
}

// Test registry functionality
TEST_F(FileSystemToolsTest, RegistryFunctionality) {
    // Check that functions are registered
    EXPECT_TRUE(registry.isRegistered("fs.join"));
    EXPECT_TRUE(registry.isRegistered("fs.exists"));
    EXPECT_TRUE(registry.isRegistered("fs.readText"));
    EXPECT_TRUE(registry.isRegistered("fs.writeText"));
    
    // Check function documentation
    auto doc = registry.getFunctionDoc("fs.readText");
    EXPECT_FALSE(doc.empty());
    EXPECT_TRUE(doc.find("Read text file contents") != std::string::npos);
    
    // Get list of all registered functions
    auto functions = registry.getRegisteredFunctions();
    EXPECT_GT(functions.size(), 15);  // Should have many functions registered
}