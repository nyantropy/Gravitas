#include "InputBindingSerializer.h"

#include <charconv>
#include <fstream>
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
        const auto action = json.findString("action");
        const auto code = json.findString("code");
        if (!action || !code)
            return std::nullopt;
        binding.action = *action;
        codeName = *code;
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
                const auto mode = gts::enumValue(activationModeNames, value.asString());
                if (!mode)
                    return std::nullopt;
                binding.mode = *mode;
            }
            else if (key == "pausePolicy")
            {
                if (!value.isString())
                    return std::nullopt;
                const auto policy = gts::enumValue(pausePolicyNames, value.asString());
                if (!policy)
                    return std::nullopt;
                binding.pausePolicy = *policy;
            }
            else if (key == "modifiers")
            {
                if (!value.isArray())
                    return std::nullopt;
                for (const auto& modifier : value.asArray())
                {
                    if (!modifier.isString())
                        return std::nullopt;
                    const auto flag = gts::enumValue(modifierFlagNames, modifier.asString());
                    if (!flag)
                        return std::nullopt;
                    binding.trigger.modifiers |= *flag;
                }
            }
        }
        const auto triggerType = gts::enumValue(inputTriggerTypeNames, type);
        if (!triggerType)
            return std::nullopt;
        binding.trigger.type = *triggerType;
        if (binding.trigger.type == InputTrigger::Type::Key)
        {
            const auto keyCode = stringToKeyCode(codeName);
            if (!keyCode)
                return std::nullopt;
            binding.trigger.code = *keyCode;
        }
        else
        {
            const auto result =
                std::from_chars(codeName.data(), codeName.data() + codeName.size(), binding.trigger.code);
            if (result.ec != std::errc{} || result.ptr != codeName.data() + codeName.size())
                return std::nullopt;
            if (binding.trigger.type == InputTrigger::Type::GamepadAxis)
                binding.trigger.axisIndex = binding.trigger.code;
        }
        return binding;
    }

} // namespace

std::optional<InputBindingDocument> parseInputBindingDocument(const std::string& source)
{
    GtsJsonValue root;
    if (!GtsJsonParser::parse(source, root) || !root.isObject())
        return std::nullopt;
    const auto version = root.findInt32("version");
    const auto* bindings = root.findArray("bindings");
    if (!version || !bindings)
        return std::nullopt;
    InputBindingDocument document;
    document.version = *version;
    for (const auto& value : *bindings)
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
        for (const auto& entry : modifierFlagNames)
            if (has(binding.trigger.modifiers, entry.value))
                modifiers.emplace_back(std::string(entry.name));
        GtsJsonValue::Object object{
            {"action", binding.action},
            {"type", std::string(gts::enumName(inputTriggerTypeNames, binding.trigger.type).value_or("key"))},
            {"code",
             binding.trigger.type == InputTrigger::Type::Key ? keyCodeToString(binding.trigger.code)
                                                             : std::to_string(binding.trigger.code)},
            {"modifiers", std::move(modifiers)},
            {"mode", std::string(gts::enumName(activationModeNames, binding.mode).value_or("pressed"))},
            {"context", binding.context},
            {"pausePolicy", std::string(gts::enumName(pausePolicyNames, binding.pausePolicy).value_or("gameplay"))}};
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
