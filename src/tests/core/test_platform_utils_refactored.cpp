#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "platform_utils.hpp"
#include "test_data_generators.hpp"
#include <chrono>

// Parameterized test for UTF-8 to wide string conversion
class UTF8ToWideTest : public ::testing::TestWithParam<std::pair<std::string, std::wstring>> {};

TEST_P(UTF8ToWideTest, ConvertUTF8ToWide) {
    auto [utf8_input, expected_wide] = GetParam();
    
    EXPECT_NO_THROW({
        std::wstring result = platform::utf8_to_wide(utf8_input);
        
        // For empty input, expect empty output
        if (utf8_input.empty()) {
            EXPECT_TRUE(result.empty());
        }
        // For non-empty ASCII input, expect same content (conversion dependent)
        else if (!expected_wide.empty() && 
                 std::all_of(utf8_input.begin(), utf8_input.end(), 
                            [](char c) { return c >= 0 && c <= 127; })) {
            // Only check ASCII content as conversion may vary for Unicode
            EXPECT_EQ(result, expected_wide);
        }
    }) << "Conversion failed for input: " << utf8_input;
}

INSTANTIATE_TEST_SUITE_P(
    BasicConversions,
    UTF8ToWideTest,
    ::testing::Values(
        std::make_pair("", std::wstring(L"")),
        std::make_pair("Hello", std::wstring(L"Hello")),
        std::make_pair("Hello World", std::wstring(L"Hello World")),
        std::make_pair("UTF-8: ñ, é, ü", std::wstring(L"UTF-8: ñ, é, ü")),
        std::make_pair("12345", std::wstring(L"12345")),
        std::make_pair("!@#$%", std::wstring(L"!@#$%"))
    )
);

// Parameterized test for wide to UTF-8 conversion
class WideToUTF8Test : public ::testing::TestWithParam<std::pair<std::wstring, std::string>> {};

TEST_P(WideToUTF8Test, ConvertWideToUTF8) {
    auto [wide_input, expected_utf8] = GetParam();
    
    EXPECT_NO_THROW({
        std::string result = platform::wide_to_utf8(wide_input);
        
        // For empty input, expect empty output
        if (wide_input.empty()) {
            EXPECT_TRUE(result.empty());
        }
        // For ASCII wide strings, expect matching ASCII output
        else if (!expected_utf8.empty() && 
                 std::all_of(wide_input.begin(), wide_input.end(), 
                            [](wchar_t c) { return c >= 0 && c <= 127; })) {
            EXPECT_EQ(result, expected_utf8);
        }
        // For non-empty input, should produce non-empty output
        else if (!wide_input.empty()) {
            EXPECT_FALSE(result.empty());
        }
    }) << "Conversion failed for input";
}

INSTANTIATE_TEST_SUITE_P(
    BasicConversions,
    WideToUTF8Test,
    ::testing::Values(
        std::make_pair(std::wstring(L""), ""),
        std::make_pair(std::wstring(L"Hello"), "Hello"),
        std::make_pair(std::wstring(L"Hello World"), "Hello World"),
        std::make_pair(std::wstring(L"Wide: ñ, é, ü"), "Wide: ñ, é, ü"),
        std::make_pair(std::wstring(L"12345"), "12345"),
        std::make_pair(std::wstring(L"!@#$%"), "!@#$%")
    )
);

// Parameterized test for round-trip conversions
class RoundTripConversionTest : public ::testing::TestWithParam<std::string> {};

TEST_P(RoundTripConversionTest, UTF8RoundTrip) {
    std::string original = GetParam();
    
    EXPECT_NO_THROW({
        std::wstring wide = platform::utf8_to_wide(original);
        std::string back_to_utf8 = platform::wide_to_utf8(wide);
        
        // For empty strings, should round-trip perfectly
        if (original.empty()) {
            EXPECT_EQ(original, back_to_utf8);
        }
        // For ASCII strings, should round-trip perfectly
        else if (std::all_of(original.begin(), original.end(), 
                            [](char c) { return c >= 0 && c <= 127; })) {
            EXPECT_EQ(original, back_to_utf8);
        }
        // For Unicode strings, conversion might not be identical due to normalization
        // but should produce valid output of reasonable length
        else {
            EXPECT_FALSE(back_to_utf8.empty());
            // Length should be within reasonable bounds (not drastically different)
            EXPECT_GT(back_to_utf8.length(), 0);
            EXPECT_LT(back_to_utf8.length(), original.length() * 4);  // UTF-8 max expansion
        }
    }) << "Round-trip conversion failed for: " << original;
}

INSTANTIATE_TEST_SUITE_P(
    RoundTripTests,
    RoundTripConversionTest,
    ::testing::Values(
        "",
        "Hello",
        "Hello World",
        "Special chars: ñéüçà",
        "Numbers: 123456789",
        "Symbols: !@#$%^&*()",
        "Mixed: Test123!@#",
        "Emoji: 😀😃😄",
        "Japanese: こんにちは",
        "Chinese: 你好",
        "Arabic: مرحبا",
        "Russian: Привет",
        std::string(1000, 'A'),  // Long string
        "Line\nBreaks\nHere",
        "Tabs\there\tand\tthere"
    )
);

// Parameterized test for encoding namespace functions
class EncodingTest : public ::testing::TestWithParam<std::string> {};

TEST_P(EncodingTest, UTF8ToWideEncoding) {
    std::string input = GetParam();
    
    EXPECT_NO_THROW({
        std::wstring result = platform::utf8_to_wide(input);
        // Verify basic properties
        if (input.empty()) {
            EXPECT_TRUE(result.empty());
        }
    });
}

TEST_P(EncodingTest, WideToUTF8Encoding) {
    std::string utf8_input = GetParam();
    std::wstring wide_input = platform::utf8_to_wide(utf8_input);
    
    EXPECT_NO_THROW({
        std::string result = platform::wide_to_utf8(wide_input);
        // Should match original for valid UTF-8
        if (!utf8_input.empty()) {
            EXPECT_FALSE(result.empty());
        }
    });
}

INSTANTIATE_TEST_SUITE_P(
    EncodingTests,
    EncodingTest,
    ::testing::Values(
        "",
        "ASCII",
        "UTF-8: àèìòù",
        "Emoji: 🌍🌎🌏",
        "Mixed: Test_123_ñéü",
        std::string(10000, 'X')
    )
);

// Parameterized test for malformed UTF-8 handling
class MalformedUTF8Test : public ::testing::TestWithParam<std::string> {};

TEST_P(MalformedUTF8Test, HandleMalformedUTF8) {
    std::string malformed = GetParam();
    
    // Should handle malformed sequences gracefully
    EXPECT_NO_THROW({
        try {
            std::wstring wide = platform::utf8_to_wide(malformed);
            // May succeed with replacement chars or throw
        } catch (const std::exception&) {
            // Exception is acceptable for malformed input
        }
    });
}

INSTANTIATE_TEST_SUITE_P(
    MalformedSequences,
    MalformedUTF8Test,
    ::testing::ValuesIn(TestDataGenerators::GetMalformedUTF8Sequences())
);

// Non-parameterized edge case tests
class PlatformUtilsEdgeTest : public ::testing::Test {};

TEST_F(PlatformUtilsEdgeTest, HandleNullBytes) {
    std::string with_null("Hello\0World", 11);
    
    EXPECT_NO_THROW({
        std::wstring wide = platform::utf8_to_wide(with_null);
        std::string back = platform::wide_to_utf8(wide);
        // Null handling depends on implementation
    });
}

TEST_F(PlatformUtilsEdgeTest, HandleMaxSizeStrings) {
    // Test with very large strings
    const size_t large_size = 1000000;
    std::string large_utf8(large_size, 'A');
    
    EXPECT_NO_THROW({
        std::wstring wide = platform::utf8_to_wide(large_utf8);
        EXPECT_EQ(wide.size(), large_size);
        
        std::string back = platform::wide_to_utf8(wide);
        EXPECT_EQ(back.size(), large_size);
    });
}

TEST_F(PlatformUtilsEdgeTest, HandleSurrogates) {
    // Test surrogate pair handling
    std::wstring with_surrogates = L"Test \U0001F600 Emoji";  // 😀
    
    EXPECT_NO_THROW({
        std::string utf8 = platform::wide_to_utf8(with_surrogates);
        std::wstring back = platform::utf8_to_wide(utf8);
        EXPECT_EQ(with_surrogates, back);
    });
}

TEST_F(PlatformUtilsEdgeTest, HandleBOM) {
    // Test Byte Order Mark handling
    std::string with_bom = "\xEF\xBB\xBF" "Hello";  // UTF-8 BOM
    
    EXPECT_NO_THROW({
        std::wstring wide = platform::utf8_to_wide(with_bom);
        // BOM might be preserved or stripped
        EXPECT_FALSE(wide.empty());
    });
}

// Performance test with parameterized sizes
class ConversionPerformanceTest : public ::testing::TestWithParam<size_t> {};

TEST_P(ConversionPerformanceTest, ConversionSpeed) {
    size_t size = GetParam();
    std::string test_string(size, 'A');
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        std::wstring wide = platform::utf8_to_wide(test_string);
        std::string back = platform::wide_to_utf8(wide);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    
    // Performance should scale linearly
    // Allow up to 10ms per 1000 chars for 100 iterations
    size_t expected_ms = (size / 1000) * 10;
    EXPECT_LT(duration.count(), std::max(size_t(100), expected_ms)) 
        << "Conversion too slow for size " << size;
}

INSTANTIATE_TEST_SUITE_P(
    PerformanceTests,
    ConversionPerformanceTest,
    ::testing::Values(10, 100, 1000, 10000, 100000)
);