#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "console_output.hpp"
#include <sstream>
#include <iostream>

class ConsoleOutputTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Capture stdout for testing
        old_cout = std::wcout.rdbuf();
        wcout_buffer.str(L"");
        std::wcout.rdbuf(wcout_buffer.rdbuf());
    }
    
    void TearDown() override {
        // Restore stdout
        std::wcout.rdbuf(old_cout);
    }
    
    std::wstringstream wcout_buffer;
    std::wstreambuf* old_cout;
};

TEST_F(ConsoleOutputTest, PrintWideString) {
    ConsoleOutput::print(L"Test message");
    
    std::wstring output = wcout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr(L"Test message"));
    EXPECT_THAT(output, ::testing::HasSubstr(L"\n"));  // Should add newline
}

TEST_F(ConsoleOutputTest, PrintWideStringNoNewline) {
    ConsoleOutput::print(L"Test message", false);
    
    std::wstring output = wcout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr(L"Test message"));
    EXPECT_THAT(output, ::testing::Not(::testing::EndsWith(L"\n")));
}

TEST_F(ConsoleOutputTest, PrintString) {
    // ASCII strings go to cout, not wcout, so we need to capture cout
    std::streambuf* old_cout = std::cout.rdbuf();
    std::stringstream cout_buffer;
    std::cout.rdbuf(cout_buffer.rdbuf());
    
    ConsoleOutput::print("ASCII test message");
    
    std::string output = cout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("ASCII test message"));
    
    std::cout.rdbuf(old_cout);
}

TEST_F(ConsoleOutputTest, PrintStringNoNewline) {
    std::streambuf* old_cout = std::cout.rdbuf();
    std::stringstream cout_buffer;
    std::cout.rdbuf(cout_buffer.rdbuf());
    
    ConsoleOutput::print("ASCII test", false);
    
    std::string output = cout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("ASCII test"));
    
    std::cout.rdbuf(old_cout);
}

TEST_F(ConsoleOutputTest, PrintEmptyString) {
    ConsoleOutput::print(L"");
    
    std::wstring output = wcout_buffer.str();
    EXPECT_EQ(output, L"\n");  // Just a newline
}

TEST_F(ConsoleOutputTest, PrintSpecialCharacters) {
    ConsoleOutput::print(L"Special: \t\n\r");
    
    std::wstring output = wcout_buffer.str();
    EXPECT_FALSE(output.empty());
}

TEST_F(ConsoleOutputTest, PrintUnicode) {
    ConsoleOutput::print(L"Unicode: 世界 🌍");
    
    std::wstring output = wcout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr(L"Unicode"));
}

TEST_F(ConsoleOutputTest, ErrorOutput) {
    // ASCII errors go to cerr, not wcerr
    std::streambuf* old_cerr = std::cerr.rdbuf();
    std::stringstream cerr_buffer;
    std::cerr.rdbuf(cerr_buffer.rdbuf());
    
    ConsoleOutput::error("Test error message");
    
    std::string error_output = cerr_buffer.str();
    EXPECT_THAT(error_output, ::testing::HasSubstr("Test error message"));
    
    // Restore stderr
    std::cerr.rdbuf(old_cerr);
}

TEST_F(ConsoleOutputTest, ErrorWithEmptyMessage) {
    std::streambuf* old_cerr = std::cerr.rdbuf();
    std::stringstream cerr_buffer;
    std::cerr.rdbuf(cerr_buffer.rdbuf());
    
    ConsoleOutput::error("");
    
    std::string error_output = cerr_buffer.str();
    // Should just be a newline
    EXPECT_EQ(error_output, "\n");
    
    std::cerr.rdbuf(old_cerr);
}

TEST_F(ConsoleOutputTest, MultipleConsecutivePrints) {
    ConsoleOutput::print(L"Line 1");
    ConsoleOutput::print(L"Line 2");
    ConsoleOutput::print(L"Line 3");
    
    std::wstring output = wcout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr(L"Line 1"));
    EXPECT_THAT(output, ::testing::HasSubstr(L"Line 2"));
    EXPECT_THAT(output, ::testing::HasSubstr(L"Line 3"));
}

TEST_F(ConsoleOutputTest, MixedPrintTypes) {
    // Need to capture both cout and wcout for mixed output
    std::streambuf* old_cout = std::cout.rdbuf();
    std::stringstream cout_buffer;
    std::cout.rdbuf(cout_buffer.rdbuf());
    
    ConsoleOutput::print(L"Wide string");
    ConsoleOutput::print("ASCII string");
    ConsoleOutput::print(L"Another wide", false);
    ConsoleOutput::print(" and ASCII", true);
    
    // Check wide output
    std::wstring wide_output = wcout_buffer.str();
    EXPECT_THAT(wide_output, ::testing::HasSubstr(L"Wide string"));
    EXPECT_THAT(wide_output, ::testing::HasSubstr(L"Another wide"));
    
    // Check ASCII output
    std::string ascii_output = cout_buffer.str();
    EXPECT_THAT(ascii_output, ::testing::HasSubstr("ASCII string"));
    EXPECT_THAT(ascii_output, ::testing::HasSubstr(" and ASCII"));
    
    std::cout.rdbuf(old_cout);
}

TEST_F(ConsoleOutputTest, VeryLongMessage) {
    std::wstring long_message(10000, L'A');
    ConsoleOutput::print(long_message);
    
    std::wstring output = wcout_buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr(long_message));
}

TEST_F(ConsoleOutputTest, PrintWithNullCharacter) {
    std::wstring with_null = L"Before";
    with_null.push_back(L'\0');
    with_null.append(L"After");
    
    ConsoleOutput::print(with_null);
    
    std::wstring output = wcout_buffer.str();
    // Should handle null character gracefully
    EXPECT_FALSE(output.empty());
}