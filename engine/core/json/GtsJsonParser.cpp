#include "GtsJsonParser.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace
{
    constexpr size_t maxDepth = 128;

    bool readUtf8(std::string_view source, size_t& position, uint32_t& codepoint)
    {
        if (position == source.size())
            return false;
        const auto first = static_cast<unsigned char>(source[position++]);
        if (first < 0x80)
        {
            codepoint = first;
            return true;
        }
        unsigned remaining = 0;
        uint32_t minimum   = 0;
        if (first >= 0xc2 && first <= 0xdf)
        {
            codepoint = first & 0x1f;
            remaining = 1;
            minimum   = 0x80;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            codepoint = first & 0x0f;
            remaining = 2;
            minimum   = 0x800;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            codepoint = first & 0x07;
            remaining = 3;
            minimum   = 0x10000;
        }
        else
            return false;
        while (remaining-- != 0)
        {
            if (position == source.size())
                return false;
            const auto byte = static_cast<unsigned char>(source[position++]);
            if ((byte & 0xc0) != 0x80)
                return false;
            codepoint = (codepoint << 6) | (byte & 0x3f);
        }
        return codepoint >= minimum && codepoint <= 0x10ffff && !(codepoint >= 0xd800 && codepoint <= 0xdfff);
    }

    void appendUtf8(std::string& output, uint32_t codepoint)
    {
        if (codepoint <= 0x7f)
            output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ff)
        {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        else if (codepoint <= 0xffff)
        {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        else
        {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    class Reader
    {
        public:
        explicit Reader(std::string_view source) : source(source) {}

        bool parse(GtsJsonValue& output, std::string* error)
        {
            GtsJsonValue parsed;
            bool         valid = readValue(parsed, 0);
            skipWhitespace();
            if (valid && position != source.size())
                valid = fail("Unexpected trailing content");
            if (error != nullptr)
                *error = valid ? "" : message + " at byte " + std::to_string(position);
            if (valid)
                output = std::move(parsed);
            return valid;
        }

        private:
        std::string_view source;
        size_t           position = 0;
        std::string      message;

        bool fail(const char* reason)
        {
            message = reason;
            return false;
        }

        void skipWhitespace()
        {
            while (position < source.size() && (source[position] == ' ' || source[position] == '\t' ||
                                                source[position] == '\n' || source[position] == '\r'))
                ++position;
        }

        bool take(char ch)
        {
            if (position < source.size() && source[position] == ch)
            {
                ++position;
                return true;
            }
            return false;
        }

        bool digit() const
        {
            return position < source.size() && source[position] >= '0' && source[position] <= '9';
        }

        bool readHex(uint32_t& value)
        {
            value = 0;
            for (unsigned i = 0; i < 4; ++i)
            {
                if (position == source.size())
                    return fail("Incomplete Unicode escape");
                const char ch = source[position++];
                unsigned   digit;
                if (ch >= '0' && ch <= '9')
                    digit = ch - '0';
                else if (ch >= 'a' && ch <= 'f')
                    digit = ch - 'a' + 10;
                else if (ch >= 'A' && ch <= 'F')
                    digit = ch - 'A' + 10;
                else
                    return fail("Invalid Unicode escape");
                value = (value << 4) | digit;
            }
            return true;
        }

        bool readString(std::string& output)
        {
            if (!take('"'))
                return fail("Expected a string");
            while (position < source.size())
            {
                const auto ch = static_cast<unsigned char>(source[position++]);
                if (ch == '"')
                    return true;
                if (ch < 0x20)
                    return fail("Unescaped control character");
                if (ch >= 0x80)
                {
                    const size_t begin = --position;
                    uint32_t     codepoint;
                    if (!readUtf8(source, position, codepoint))
                        return fail("Invalid UTF-8");
                    output.append(source.substr(begin, position - begin));
                    continue;
                }
                if (ch != '\\')
                {
                    output.push_back(static_cast<char>(ch));
                    continue;
                }
                if (position == source.size())
                    return fail("Incomplete escape");
                switch (source[position++])
                {
                case '"':
                    output.push_back('"');
                    break;
                case '\\':
                    output.push_back('\\');
                    break;
                case '/':
                    output.push_back('/');
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u':
                {
                    uint32_t codepoint;
                    if (!readHex(codepoint))
                        return false;
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff)
                    {
                        if (!take('\\') || !take('u'))
                            return fail("Missing low surrogate");
                        uint32_t low;
                        if (!readHex(low))
                            return false;
                        if (low < 0xdc00 || low > 0xdfff)
                            return fail("Invalid low surrogate");
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                    }
                    else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
                        return fail("Unpaired low surrogate");
                    appendUtf8(output, codepoint);
                    break;
                }
                default:
                    return fail("Invalid escape");
                }
            }
            return fail("Unterminated string");
        }

        bool readNumber(GtsJsonValue& output)
        {
            const size_t begin = position;
            take('-');
            if (!take('0'))
            {
                if (!digit())
                    return fail("Expected a digit");
                while (digit())
                    ++position;
            }
            if (take('.'))
            {
                if (!digit())
                    return fail("Expected fractional digits");
                while (digit())
                    ++position;
            }
            if (take('e') || take('E'))
            {
                if (!take('+'))
                    take('-');
                if (!digit())
                    return fail("Expected exponent digits");
                while (digit())
                    ++position;
            }
            const auto token = source.substr(begin, position - begin);
            if (token.find_first_of(".eE") == std::string_view::npos && token != "-0")
            {
                if (token.front() == '-')
                {
                    int64_t    number;
                    const auto result = std::from_chars(token.data(), token.data() + token.size(), number);
                    if (result.ec != std::errc{})
                        return fail("Integer is out of range");
                    output.value = number;
                }
                else
                {
                    uint64_t   number;
                    const auto result = std::from_chars(token.data(), token.data() + token.size(), number);
                    if (result.ec != std::errc{})
                        return fail("Integer is out of range");
                    output.value = number;
                }
                return true;
            }
            double     number;
            const auto result = std::from_chars(source.data() + begin, source.data() + position, number);
            if (result.ec != std::errc{} || result.ptr != source.data() + position || !std::isfinite(number))
                return fail("Number is out of range");
            output.value = number;
            return true;
        }

        bool readValue(GtsJsonValue& output, size_t depth)
        {
            skipWhitespace();
            if (position == source.size())
                return fail("Unexpected end of JSON");
            if (depth > maxDepth)
                return fail("Nesting limit exceeded");
            if (take('{'))
            {
                GtsJsonValue::Object            object;
                std::unordered_set<std::string> keys;
                skipWhitespace();
                if (!take('}'))
                {
                    do
                    {
                        skipWhitespace();
                        std::string key;
                        if (!readString(key))
                            return false;
                        if (!keys.insert(key).second)
                            return fail("Duplicate object key");
                        skipWhitespace();
                        if (!take(':'))
                            return fail("Expected ':'");
                        GtsJsonValue member;
                        if (!readValue(member, depth + 1))
                            return false;
                        object.emplace_back(std::move(key), std::move(member));
                        skipWhitespace();
                        if (take('}'))
                        {
                            output.value = std::move(object);
                            return true;
                        }
                    } while (take(','));
                    return fail("Expected ',' or '}'");
                }
                output.value = std::move(object);
                return true;
            }
            if (take('['))
            {
                GtsJsonValue::Array array;
                skipWhitespace();
                if (!take(']'))
                {
                    do
                    {
                        GtsJsonValue item;
                        if (!readValue(item, depth + 1))
                            return false;
                        array.push_back(std::move(item));
                        skipWhitespace();
                        if (take(']'))
                        {
                            output.value = std::move(array);
                            return true;
                        }
                    } while (take(','));
                    return fail("Expected ',' or ']'");
                }
                output.value = std::move(array);
                return true;
            }
            if (source[position] == '"')
            {
                std::string string;
                if (!readString(string))
                    return false;
                output.value = std::move(string);
                return true;
            }
            for (const auto literal : {"true", "false", "null"})
            {
                const std::string_view token(literal);
                if (source.substr(position, token.size()) == token)
                {
                    position += token.size();
                    if (token != "null")
                        output.value = token == "true";
                    return true;
                }
            }
            if (source[position] == '-' || digit())
                return readNumber(output);
            return fail("Unexpected token");
        }
    };

    void writeString(std::string& output, std::string_view string)
    {
        output.push_back('"');
        constexpr char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < string.size();)
        {
            const size_t begin = i;
            uint32_t     codepoint;
            if (!readUtf8(string, i, codepoint))
                throw std::invalid_argument("Cannot serialize invalid UTF-8");
            if (codepoint == '"' || codepoint == '\\')
            {
                output.push_back('\\');
                output.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint < 0x20)
            {
                output += "\\u00";
                output.push_back(hex[codepoint >> 4]);
                output.push_back(hex[codepoint & 15]);
            }
            else
                output.append(string.substr(begin, i - begin));
        }
        output.push_back('"');
    }

    void writeValue(std::string& output, const GtsJsonValue& value, int indent, size_t depth)
    {
        if (depth > maxDepth)
            throw std::invalid_argument("JSON nesting limit exceeded");
        if (value.isNull())
        {
            output += "null";
            return;
        }
        if (value.isBool())
        {
            output += value.asBool() ? "true" : "false";
            return;
        }
        if (value.isString())
        {
            writeString(output, value.asString());
            return;
        }
        if (value.isNumber())
        {
            const double number = value.asNumber();
            if (!std::isfinite(number))
                throw std::invalid_argument("Cannot serialize a non-finite JSON number");
            char       buffer[64];
            const auto result = value.isInteger() ? std::to_chars(buffer, buffer + sizeof(buffer), value.asInteger())
                                : value.isUnsignedInteger()
                                    ? std::to_chars(buffer, buffer + sizeof(buffer), value.asUnsignedInteger())
                                    : std::to_chars(buffer, buffer + sizeof(buffer), number);
            if (result.ec != std::errc{})
                throw std::invalid_argument("Cannot serialize JSON number");
            output.append(buffer, result.ptr);
            return;
        }
        const bool   object = value.isObject();
        const size_t size   = object ? value.asObject().size() : value.asArray().size();
        output.push_back(object ? '{' : '[');
        std::unordered_set<std::string_view> keys;
        for (size_t i = 0; i < size; ++i)
        {
            output += i == 0 ? "\n" : ",\n";
            output.append(static_cast<size_t>(indent + 2), ' ');
            if (object)
            {
                const auto& [key, member] = value.asObject()[i];
                if (!keys.insert(key).second)
                    throw std::invalid_argument("Cannot serialize duplicate JSON keys");
                writeString(output, key);
                output += ": ";
                writeValue(output, member, indent + 2, depth + 1);
            }
            else
                writeValue(output, value.asArray()[i], indent + 2, depth + 1);
        }
        if (size != 0)
        {
            output += '\n';
            output.append(static_cast<size_t>(indent), ' ');
        }
        output.push_back(object ? '}' : ']');
    }
}


bool GtsJsonParser::parse(std::string_view source, GtsJsonValue& output, std::string* error)
{
    return Reader(source).parse(output, error);
}

std::string GtsJsonParser::serialize(const GtsJsonValue& value, int indent)
{
    if (indent < 0 || indent > 256)
        throw std::invalid_argument("JSON indentation is out of range");
    std::string output;
    writeValue(output, value, indent, 0);
    return output;
}
