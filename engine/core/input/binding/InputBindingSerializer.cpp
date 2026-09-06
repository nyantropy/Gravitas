#include "InputBindingSerializer.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "GtsJsonParser.h"
#include "InputKeyNames.h"

namespace
{
    std::optional<InputBinding> readBinding(const GtsJsonValue& json)
    {
        if (!json.isObject())
            return std::nullopt;
        InputBinding binding;
        std::string  type = "key";
        std::string  codeName;
        const auto*  action = json.find("action");
        const auto*  code   = json.find("code");
        if (!action || !action->isString() || !code || !code->isString())
            return std::nullopt;
        binding.action = action->asString();
        codeName       = code->asString();
        for (const auto& [key, value] : json.asObject())
        {
            if (key == "type")
            {
                if (!value.isString())
                    return std::nullopt;
                type = value.asString();
            }
            else if (key == "context")
            {
                if (!value.isString())
                    return std::nullopt;
                binding.context = value.asString();
            }
            else if (key == "passthrough")
            {
                if (!value.isBool())
                    return std::nullopt;
                binding.passthrough = value.asBool();
            }
            else if (key == "mode")
            {
                if (!value.isString())
                    return std::nullopt;
                const auto& mode = value.asString();
                if (mode == "pressed")
                    binding.mode = ActivationMode::Pressed;
                else if (mode == "released")
                    binding.mode = ActivationMode::Released;
                else if (mode == "held")
                    binding.mode = ActivationMode::Held;
                else if (mode == "repeated")
                    binding.mode = ActivationMode::Repeated;
                else
                    return std::nullopt;
            }
            else if (key == "pausePolicy")
            {
                if (!value.isString())
                    return std::nullopt;
                if (value.asString() == "gameplay")
                    binding.pausePolicy = PausePolicy::Gameplay;
                else if (value.asString() == "always_active")
                    binding.pausePolicy = PausePolicy::AlwaysActive;
                else
                    return std::nullopt;
            }
            else if (key == "modifiers")
            {
                if (!value.isArray())
                    return std::nullopt;
                for (const auto& modifier : value.asArray())
                {
                    if (!modifier.isString())
                        return std::nullopt;
                    if (modifier.asString() == "shift")
                        binding.trigger.modifiers |= ModifierFlags::Shift;
                    else if (modifier.asString() == "ctrl")
                        binding.trigger.modifiers |= ModifierFlags::Ctrl;
                    else if (modifier.asString() == "alt")
                        binding.trigger.modifiers |= ModifierFlags::Alt;
                    else if (modifier.asString() == "super")
                        binding.trigger.modifiers |= ModifierFlags::Super;
                    else
                        return std::nullopt;
                }
            }
        }
        if (type == "key")
        {
            const auto keyCode = stringToKeyCode(codeName);
            if (!keyCode)
                return std::nullopt;
            binding.trigger.code = *keyCode;
        }
        else
        {
            if (type == "mouse_button")
                binding.trigger.type = InputTrigger::Type::MouseButton;
            else if (type == "gamepad_button")
                binding.trigger.type = InputTrigger::Type::GamepadButton;
            else if (type == "gamepad_axis")
                binding.trigger.type = InputTrigger::Type::GamepadAxis;
            else
                return std::nullopt;
            const auto result =
                std::from_chars(codeName.data(), codeName.data() + codeName.size(), binding.trigger.code);
            if (result.ec != std::errc{} || result.ptr != codeName.data() + codeName.size())
                return std::nullopt;
            if (type == "gamepad_axis")
                binding.trigger.axisIndex = binding.trigger.code;
        }
        return binding;
    }

    const char* activationModeToString(ActivationMode mode)
    {
        switch (mode)
        {
        case ActivationMode::Pressed:
            return "pressed";
        case ActivationMode::Released:
            return "released";
        case ActivationMode::Held:
            return "held";
        case ActivationMode::Repeated:
            return "repeated";
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
        case InputTrigger::Type::Key:
            return "key";
        case InputTrigger::Type::MouseButton:
            return "mouse_button";
        case InputTrigger::Type::GamepadButton:
            return "gamepad_button";
        case InputTrigger::Type::GamepadAxis:
            return "gamepad_axis";
        }

        return "key";
    }
} // namespace

std::optional<InputBindingDocument> parseInputBindingDocument(const std::string& source)
{
    GtsJsonValue root;
    if (!GtsJsonParser::parse(source, root) || !root.isObject())
        return std::nullopt;
    const auto* version  = root.find("version");
    const auto* bindings = root.find("bindings");
    if (!version || !version->isNumber() || !bindings || !bindings->isArray())
        return std::nullopt;
    const double number = version->asNumber();
    if (std::trunc(number) != number || number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max())
        return std::nullopt;
    InputBindingDocument document;
    document.version = static_cast<int>(number);
    for (const auto& value : bindings->asArray())
    {
        const auto binding = readBinding(value);
        if (!binding)
            return std::nullopt;
        document.bindings.push_back(*binding);
    }
    return document;
}

std::string serializeInputBindingDocument(const std::vector<InputBinding>& bindings, int version)
{
    GtsJsonValue::Array array;
    for (const auto& binding : bindings)
    {
        GtsJsonValue::Array modifiers;
        if (has(binding.trigger.modifiers, ModifierFlags::Shift))
            modifiers.emplace_back("shift");
        if (has(binding.trigger.modifiers, ModifierFlags::Ctrl))
            modifiers.emplace_back("ctrl");
        if (has(binding.trigger.modifiers, ModifierFlags::Alt))
            modifiers.emplace_back("alt");
        if (has(binding.trigger.modifiers, ModifierFlags::Super))
            modifiers.emplace_back("super");
        GtsJsonValue::Object object{{"action", binding.action},
                                    {"type", triggerTypeToString(binding.trigger.type)},
                                    {"code",
                                     binding.trigger.type == InputTrigger::Type::Key
                                         ? keyCodeToString(binding.trigger.code)
                                         : std::to_string(binding.trigger.code)},
                                    {"modifiers", std::move(modifiers)},
                                    {"mode", activationModeToString(binding.mode)},
                                    {"context", binding.context},
                                    {"pausePolicy", pausePolicyToString(binding.pausePolicy)}};
        if (binding.passthrough)
            object.emplace_back("passthrough", true);
        array.emplace_back(std::move(object));
    }
    return GtsJsonParser::serialize(GtsJsonValue::Object{{"version", version}, {"bindings", std::move(array)}}) + "\n";
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

bool saveInputBindingDocumentToFile(const std::string& path, const std::vector<InputBinding>& bindings, int version)
{
    std::string json;
    try
    {
        json = serializeInputBindingDocument(bindings, version);
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
        return false;

    file << json;
    return file.good();
}
