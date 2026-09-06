#include "GtsJsonParser.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void rejects(std::string_view source)
    {
        GtsJsonValue value("unchanged");
        std::string  error;
        require(!GtsJsonParser::parse(source, value, &error), "Accepted invalid JSON: " + std::string(source));
        require(value.isString() && value.asString() == "unchanged", "Failure modified output");
        require(error.find("byte") != std::string::npos, "Missing error position");
    }

    void rejectsWrite(const GtsJsonValue& value)
    {
        try
        {
            GtsJsonParser::serialize(value);
        }
        catch (const std::invalid_argument&)
        {
            return;
        }
        throw std::runtime_error("Serialized invalid value");
    }

    void checkFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        require(file.good(), "Cannot open " + path.string());
        std::ostringstream source;
        source << file.rdbuf();
        GtsJsonValue value;
        std::string  error;
        require(GtsJsonParser::parse(source.str(), value, &error), path.string() + ": " + error);
        GtsJsonValue restored;
        require(GtsJsonParser::parse(GtsJsonParser::serialize(value), restored), "Asset roundtrip failed");
        require(GtsJsonParser::serialize(restored) == GtsJsonParser::serialize(value), "Asset changed on roundtrip");
    }
}

int main(int argc, char** argv)
{
    try
    {
        for (const char* source : {"null",
                                   "true",
                                   "false",
                                   "0",
                                   "-0",
                                   "-12",
                                   "1.25",
                                   "1e+2",
                                   "1E-2",
                                   "\"\"",
                                   "[]",
                                   "{}",
                                   " \r\n\t[null, true, false, {\"a\": 12}] \t"})
        {
            GtsJsonValue value;
            std::string  error = "old";
            require(GtsJsonParser::parse(source, value, &error), std::string("Rejected: ") + source);
            require(error.empty(), "Success did not clear error");
            GtsJsonValue restored;
            require(GtsJsonParser::parse(GtsJsonParser::serialize(value), restored), "Roundtrip failed");
            require(GtsJsonParser::serialize(restored) == GtsJsonParser::serialize(value), "Unstable roundtrip");
        }
        for (const char* source : {"",
                                   " ",
                                   "01",
                                   "-01",
                                   "-",
                                   "+1",
                                   ".1",
                                   "1.",
                                   "1e",
                                   "1e+",
                                   "NaN",
                                   "Infinity",
                                   "1e999",
                                   "1e-999",
                                   "[1,]",
                                   "{\"a\":1,}",
                                   "{\"a\" 1}",
                                   "[true false]",
                                   "{}[]",
                                   "True",
                                   "nullx",
                                   "\vnull",
                                   "/*x*/null",
                                   "{\"a\":1,\"\\u0061\":2}",
                                   "\"\\q\"",
                                   "\"\\u12xx\"",
                                   "\"\\u123\"",
                                   "\"\\ud800\"",
                                   "\"\\udc00\"",
                                   "\"\\ud800\\u0041\"",
                                   "\"\n\"",
                                   "\"unterminated",
                                   "\"\xc0\xaf\"",
                                   "\"\xed\xa0\x80\"",
                                   "\"\xf4\x90\x80\x80\"",
                                   "\"\x80\"",
                                   "\"\xe2\x82\"",
                                   "18446744073709551616",
                                   "-9223372036854775809"})
            rejects(source);

        GtsJsonValue value;
        require(GtsJsonParser::parse(R"("\u0000\b\f\n\r\t\/\\\"\u00e4\u20ac\ud83d\ude80")", value), "Escapes");
        const std::string expected = std::string(1, '\0') + "\b\f\n\r\t/\\\"\xc3\xa4\xe2\x82\xac\xf0\x9f\x9a\x80";
        require(value.asString() == expected, "Unicode decoding");
        GtsJsonValue restored;
        require(GtsJsonParser::parse(GtsJsonParser::serialize(value), restored) && restored.asString() == expected,
                "Unicode roundtrip");
        std::string controls;
        for (int ch = 0; ch < 32; ++ch)
            controls += static_cast<char>(ch);
        require(GtsJsonParser::parse(GtsJsonParser::serialize(controls), restored) && restored.asString() == controls,
                "Control escaping");

        require(GtsJsonParser::parse("18446744073709551615", value) &&
                    value.asUnsignedInteger() == std::numeric_limits<uint64_t>::max(),
                "Unsigned precision");
        require(GtsJsonParser::serialize(value) == "18446744073709551615", "Unsigned write precision");
        require(GtsJsonParser::parse("-9223372036854775808", value) &&
                    value.asInteger() == std::numeric_limits<int64_t>::min(),
                "Signed precision");
        require(GtsJsonParser::serialize(value) == "-9223372036854775808", "Signed write precision");
        require(GtsJsonParser::parse("-0", value) && std::signbit(value.asNumber()), "Negative zero");
        for (double number :
             {0.12345678901234567, std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()})
            require(GtsJsonParser::parse(GtsJsonParser::serialize(number), value) && value.asNumber() == number,
                    "Floating point precision");

        const GtsJsonValue object = GtsJsonValue::Object{{"first", true}, {"second", GtsJsonValue::Array{1, 2}}};
        require(object.find("first")->asBool() && object.find("missing") == nullptr, "Object lookup");
        require(GtsJsonValue(1).find("key") == nullptr, "Non-object lookup");
        require(GtsJsonParser::serialize(object).find("first") < GtsJsonParser::serialize(object).find("second"),
                "Object ordering");
        require(GtsJsonParser::parse(std::string(128, '[') + "0" + std::string(128, ']'), value), "Depth boundary");
        GtsJsonParser::serialize(value);
        rejects(std::string(129, '[') + "0" + std::string(129, ']'));
        rejectsWrite(GtsJsonValue::Array{value});
        rejectsWrite(std::numeric_limits<double>::infinity());
        rejectsWrite(std::numeric_limits<double>::quiet_NaN());
        rejectsWrite(std::string("\xc0\xaf"));
        rejectsWrite(GtsJsonValue::Object{{"key", 1}, {"key", 2}});

        size_t count = 0;
        for (int i = 1; i < argc; ++i)
        {
            const std::filesystem::path root(argv[i]);
            require(std::filesystem::exists(root), "Missing corpus: " + root.string());
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            {
                if (!entry.is_regular_file())
                    continue;
                const auto extension = entry.path().extension();
                if (extension != ".json" && extension != ".gltf")
                    continue;
                checkFile(entry.path());
                ++count;
            }
        }
        std::cout << "JSON checks passed; " << count << " asset documents roundtripped\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
