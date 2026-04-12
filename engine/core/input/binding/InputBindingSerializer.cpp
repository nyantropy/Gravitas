#include "InputBindingSerializer.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

#include "InputKeyNames.h"

namespace
{
    class JsonParser
    {
    public:
        explicit JsonParser(std::string_view source)
            : source(source)
        {
        }

        std::optional<InputBindingDocument> parseDocument()
        {
            InputBindingDocument document;

            if (!consume('{'))
                return std::nullopt;

            bool seenVersion = false;
            bool seenBindings = false;

            while (!peek('}'))
            {
                const auto key = parseString();
                if (!key.has_value())
                    return std::nullopt;
                if (!consume(':'))
                    return std::nullopt;

                if (*key == "version")
                {
                    const auto version = parseInt();
                    if (!version.has_value())
                        return std::nullopt;
                    document.version = *version;
                    seenVersion = true;
                }
                else if (*key == "bindings")
                {
                    const auto bindings = parseBindingsArray();
                    if (!bindings.has_value())
                        return std::nullopt;
                    document.bindings = std::move(*bindings);
                    seenBindings = true;
                }
                else
                {
                    if (!skipValue())
                        return std::nullopt;
                }

                if (peek(','))
                    consume(',');
                else
                    break;
            }

            if (!consume('}'))
                return std::nullopt;

            skipWhitespace();
            if (!eof() || !seenBindings || !seenVersion)
                return std::nullopt;

            return document;
        }

    private:
        std::string_view source;
        size_t position = 0;

        bool eof() const
        {
            return position >= source.size();
        }

        void skipWhitespace()
        {
            while (!eof() && std::isspace(static_cast<unsigned char>(source[position])))
                ++position;
        }

        bool peek(char ch)
        {
            skipWhitespace();
            return !eof() && source[position] == ch;
        }

        bool consume(char ch)
        {
            skipWhitespace();
            if (eof() || source[position] != ch)
                return false;

            ++position;
            return true;
        }

        std::optional<std::string> parseString()
        {
            skipWhitespace();
            if (eof() || source[position] != '"')
                return std::nullopt;

            ++position;
            std::string result;
            while (!eof())
            {
                const char ch = source[position++];
                if (ch == '"')
                    return result;

                if (ch == '\\')
                {
                    if (eof())
                        return std::nullopt;

                    const char escaped = source[position++];
                    switch (escaped)
                    {
                        case '"': result.push_back('"'); break;
                        case '\\': result.push_back('\\'); break;
                        case '/': result.push_back('/'); break;
                        case 'b': result.push_back('\b'); break;
                        case 'f': result.push_back('\f'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        default: return std::nullopt;
                    }
                    continue;
                }

                result.push_back(ch);
            }

            return std::nullopt;
        }

        std::optional<int> parseInt()
        {
            skipWhitespace();
            if (eof())
                return std::nullopt;

            size_t end = position;
            if (source[end] == '-')
                ++end;

            while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
                ++end;

            if (end == position || (end == position + 1 && source[position] == '-'))
                return std::nullopt;

            int value = 0;
            try
            {
                value = std::stoi(std::string(source.substr(position, end - position)));
            }
            catch (...)
            {
                return std::nullopt;
            }

            position = end;
            return value;
        }

        std::optional<bool> parseBool()
        {
            skipWhitespace();
            if (source.substr(position, 4) == "true")
            {
                position += 4;
                return true;
            }

            if (source.substr(position, 5) == "false")
            {
                position += 5;
                return false;
            }

            return std::nullopt;
        }

        std::optional<std::vector<std::string>> parseStringArray()
        {
            if (!consume('['))
                return std::nullopt;

            std::vector<std::string> result;
            while (!peek(']'))
            {
                const auto value = parseString();
                if (!value.has_value())
                    return std::nullopt;
                result.push_back(*value);

                if (peek(','))
                    consume(',');
                else
                    break;
            }

            if (!consume(']'))
                return std::nullopt;

            return result;
        }

        std::optional<std::vector<InputBinding>> parseBindingsArray()
        {
            if (!consume('['))
                return std::nullopt;

            std::vector<InputBinding> result;
            while (!peek(']'))
            {
                const auto binding = parseBinding();
                if (!binding.has_value())
                    return std::nullopt;
                result.push_back(std::move(*binding));

                if (peek(','))
                    consume(',');
                else
                    break;
            }

            if (!consume(']'))
                return std::nullopt;

            return result;
        }

        std::optional<InputBinding> parseBinding()
        {
            if (!consume('{'))
                return std::nullopt;

            InputBinding binding;
            std::string type = "key";
            std::string codeName;
            bool sawAction = false;
            bool sawCode = false;

            while (!peek('}'))
            {
                const auto key = parseString();
                if (!key.has_value())
                    return std::nullopt;
                if (!consume(':'))
                    return std::nullopt;

                if (*key == "action")
                {
                    const auto action = parseString();
                    if (!action.has_value())
                        return std::nullopt;
                    binding.action = *action;
                    sawAction = true;
                }
                else if (*key == "type")
                {
                    const auto parsedType = parseString();
                    if (!parsedType.has_value())
                        return std::nullopt;
                    type = *parsedType;
                }
                else if (*key == "code")
                {
                    const auto code = parseString();
                    if (!code.has_value())
                        return std::nullopt;
                    codeName = *code;
                    sawCode = true;
                }
                else if (*key == "modifiers")
                {
                    const auto modifiers = parseStringArray();
                    if (!modifiers.has_value())
                        return std::nullopt;
                    binding.trigger.modifiers = ModifierFlags::None;
                    for (const auto& modifier : *modifiers)
                    {
                        if (modifier == "shift")
                            binding.trigger.modifiers |= ModifierFlags::Shift;
                        else if (modifier == "ctrl")
                            binding.trigger.modifiers |= ModifierFlags::Ctrl;
                        else if (modifier == "alt")
                            binding.trigger.modifiers |= ModifierFlags::Alt;
                        else if (modifier == "super")
                            binding.trigger.modifiers |= ModifierFlags::Super;
                        else
                            return std::nullopt;
                    }
                }
                else if (*key == "mode")
                {
                    const auto mode = parseString();
                    if (!mode.has_value())
                        return std::nullopt;

                    if (*mode == "pressed")
                        binding.mode = ActivationMode::Pressed;
                    else if (*mode == "released")
                        binding.mode = ActivationMode::Released;
                    else if (*mode == "held")
                        binding.mode = ActivationMode::Held;
                    else if (*mode == "repeated")
                        binding.mode = ActivationMode::Repeated;
                    else
                        return std::nullopt;
                }
                else if (*key == "context")
                {
                    const auto context = parseString();
                    if (!context.has_value())
                        return std::nullopt;
                    binding.context = *context;
                }
                else if (*key == "pausePolicy")
                {
                    const auto pausePolicy = parseString();
                    if (!pausePolicy.has_value())
                        return std::nullopt;

                    if (*pausePolicy == "gameplay")
                        binding.pausePolicy = PausePolicy::Gameplay;
                    else if (*pausePolicy == "always_active")
                        binding.pausePolicy = PausePolicy::AlwaysActive;
                    else
                        return std::nullopt;
                }
                else if (*key == "passthrough")
                {
                    const auto passthrough = parseBool();
                    if (!passthrough.has_value())
                        return std::nullopt;
                    binding.passthrough = *passthrough;
                }
                else
                {
                    if (!skipValue())
                        return std::nullopt;
                }

                if (peek(','))
                    consume(',');
                else
                    break;
            }

            if (!consume('}'))
                return std::nullopt;

            if (!sawAction || !sawCode)
                return std::nullopt;

            if (type == "key")
            {
                binding.trigger.type = InputTrigger::Type::Key;
                const auto keyCode = stringToKeyCode(codeName);
                if (!keyCode.has_value())
                    return std::nullopt;
                binding.trigger.code = *keyCode;
            }
            else if (type == "mouse_button")
            {
                binding.trigger.type = InputTrigger::Type::MouseButton;
                const auto code = parseNumericString(codeName);
                if (!code.has_value())
                    return std::nullopt;
                binding.trigger.code = *code;
            }
            else if (type == "gamepad_button")
            {
                binding.trigger.type = InputTrigger::Type::GamepadButton;
                const auto code = parseNumericString(codeName);
                if (!code.has_value())
                    return std::nullopt;
                binding.trigger.code = *code;
            }
            else if (type == "gamepad_axis")
            {
                binding.trigger.type = InputTrigger::Type::GamepadAxis;
                const auto code = parseNumericString(codeName);
                if (!code.has_value())
                    return std::nullopt;
                binding.trigger.code = *code;
                binding.trigger.axisIndex = *code;
            }
            else
            {
                return std::nullopt;
            }

            return binding;
        }

        static std::optional<int> parseNumericString(const std::string& text)
        {
            try
            {
                return std::stoi(text);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        bool skipValue()
        {
            skipWhitespace();
            if (eof())
                return false;

            const char ch = source[position];
            if (ch == '"')
                return parseString().has_value();
            if (ch == '{')
            {
                consume('{');
                while (!peek('}'))
                {
                    if (!parseString().has_value())
                        return false;
                    if (!consume(':'))
                        return false;
                    if (!skipValue())
                        return false;
                    if (peek(','))
                        consume(',');
                    else
                        break;
                }
                return consume('}');
            }
            if (ch == '[')
            {
                consume('[');
                while (!peek(']'))
                {
                    if (!skipValue())
                        return false;
                    if (peek(','))
                        consume(',');
                    else
                        break;
                }
                return consume(']');
            }

            if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-')
                return parseInt().has_value();

            return parseBool().has_value();
        }
    };

    std::string escapeJson(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (char ch : value)
        {
            switch (ch)
            {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped.push_back(ch); break;
            }
        }
        return escaped;
    }

    const char* activationModeToString(ActivationMode mode)
    {
        switch (mode)
        {
            case ActivationMode::Pressed: return "pressed";
            case ActivationMode::Released: return "released";
            case ActivationMode::Held: return "held";
            case ActivationMode::Repeated: return "repeated";
        }

        return "pressed";
    }

    const char* pausePolicyToString(PausePolicy policy)
    {
        return policy == PausePolicy::AlwaysActive ? "always_active" : "gameplay";
    }

    const char* triggerTypeToString(InputTrigger::Type type)
    {
        switch (type)
        {
            case InputTrigger::Type::Key: return "key";
            case InputTrigger::Type::MouseButton: return "mouse_button";
            case InputTrigger::Type::GamepadButton: return "gamepad_button";
            case InputTrigger::Type::GamepadAxis: return "gamepad_axis";
        }

        return "key";
    }
}

std::optional<InputBindingDocument> parseInputBindingDocument(const std::string& json)
{
    JsonParser parser(json);
    return parser.parseDocument();
}

std::string serializeInputBindingDocument(const std::vector<InputBinding>& bindings, int version)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": " << version << ",\n";
    out << "  \"bindings\": [\n";

    for (size_t i = 0; i < bindings.size(); ++i)
    {
        const auto& binding = bindings[i];
        out << "    {\n";
        out << "      \"action\": \"" << escapeJson(binding.action) << "\",\n";
        out << "      \"type\": \"" << triggerTypeToString(binding.trigger.type) << "\",\n";
        if (binding.trigger.type == InputTrigger::Type::Key)
            out << "      \"code\": \"" << keyCodeToString(binding.trigger.code) << "\",\n";
        else
            out << "      \"code\": \"" << binding.trigger.code << "\",\n";

        out << "      \"modifiers\": [";
        bool wroteModifier = false;
        if (has(binding.trigger.modifiers, ModifierFlags::Shift))
        {
            out << "\"shift\"";
            wroteModifier = true;
        }
        if (has(binding.trigger.modifiers, ModifierFlags::Ctrl))
        {
            out << (wroteModifier ? ", " : "") << "\"ctrl\"";
            wroteModifier = true;
        }
        if (has(binding.trigger.modifiers, ModifierFlags::Alt))
        {
            out << (wroteModifier ? ", " : "") << "\"alt\"";
            wroteModifier = true;
        }
        if (has(binding.trigger.modifiers, ModifierFlags::Super))
            out << (wroteModifier ? ", " : "") << "\"super\"";
        out << "],\n";

        out << "      \"mode\": \"" << activationModeToString(binding.mode) << "\",\n";
        out << "      \"context\": \"" << escapeJson(binding.context) << "\",\n";
        out << "      \"pausePolicy\": \"" << pausePolicyToString(binding.pausePolicy) << "\"";
        if (binding.passthrough)
            out << ",\n      \"passthrough\": true\n";
        else
            out << "\n";

        out << "    }";
        if (i + 1 < bindings.size())
            out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
    return out.str();
}

bool loadInputBindingDocumentFromFile(const std::string& path, InputBindingDocument& document)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const auto parsed = parseInputBindingDocument(buffer.str());
    if (!parsed.has_value())
        return false;

    document = *parsed;
    return true;
}

bool saveInputBindingDocumentToFile(const std::string& path,
                                    const std::vector<InputBinding>& bindings,
                                    int version)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
        return false;

    file << serializeInputBindingDocument(bindings, version);
    return file.good();
}
