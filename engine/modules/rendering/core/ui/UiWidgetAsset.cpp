#include "UiWidgetAsset.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "UiSystem.h"

namespace
{
    using Object = UiJsonValue::Object;
    using Array = UiJsonValue::Array;

    const Object* asObject(const UiJsonValue& value)
    {
        return std::get_if<Object>(&value.value);
    }

    const Array* asArray(const UiJsonValue& value)
    {
        return std::get_if<Array>(&value.value);
    }

    std::optional<std::string> getString(const UiJsonValue& object, const std::string& key)
    {
        const UiJsonValue* value = object.find(key);
        if (value == nullptr)
            return std::nullopt;
        if (const auto* string = std::get_if<std::string>(&value->value))
            return *string;
        return std::nullopt;
    }

    std::optional<double> getNumber(const UiJsonValue& object, const std::string& key)
    {
        const UiJsonValue* value = object.find(key);
        if (value == nullptr)
            return std::nullopt;
        if (const auto* number = std::get_if<double>(&value->value))
            return *number;
        return std::nullopt;
    }

    std::optional<bool> getBool(const UiJsonValue& object, const std::string& key)
    {
        const UiJsonValue* value = object.find(key);
        if (value == nullptr)
            return std::nullopt;
        if (const auto* boolean = std::get_if<bool>(&value->value))
            return *boolean;
        return std::nullopt;
    }

    int intOr(const UiJsonValue& object, const std::string& key, int fallback)
    {
        const auto value = getNumber(object, key);
        return value.has_value() ? static_cast<int>(*value) : fallback;
    }

    bool boolOr(const UiJsonValue& object, const std::string& key, bool fallback)
    {
        const auto value = getBool(object, key);
        return value.value_or(fallback);
    }

    std::string stringOr(const UiJsonValue& object,
                         const std::string& key,
                         const std::string& fallback = {})
    {
        const auto value = getString(object, key);
        return value.value_or(fallback);
    }

    UiJsonValue jsonString(const std::string& value)
    {
        UiJsonValue json;
        json.value = value;
        return json;
    }

    UiJsonValue jsonNumber(double value)
    {
        UiJsonValue json;
        json.value = value;
        return json;
    }

    UiJsonValue jsonBool(bool value)
    {
        UiJsonValue json;
        json.value = value;
        return json;
    }

    UiJsonValue jsonArray(Array array)
    {
        UiJsonValue json;
        json.value = std::move(array);
        return json;
    }

    UiJsonValue jsonObject(Object object)
    {
        UiJsonValue json;
        json.value = std::move(object);
        return json;
    }

    std::string scalarToString(const UiJsonValue& value)
    {
        if (const auto* string = std::get_if<std::string>(&value.value))
            return *string;
        if (const auto* number = std::get_if<double>(&value.value))
        {
            std::ostringstream out;
            out << *number;
            return out.str();
        }
        if (const auto* boolean = std::get_if<bool>(&value.value))
            return *boolean ? "true" : "false";
        return {};
    }

    std::vector<std::string> parseStringArray(const UiJsonValue* value)
    {
        std::vector<std::string> result;
        const Array* array = value == nullptr ? nullptr : asArray(*value);
        if (array == nullptr)
            return result;
        for (const UiJsonValue& item : *array)
        {
            if (const auto* string = std::get_if<std::string>(&item.value))
                result.push_back(*string);
        }
        return result;
    }

    UiJsonValue serializeStringArray(const std::vector<std::string>& values)
    {
        Array array;
        for (const std::string& value : values)
            array.push_back(jsonString(value));
        return jsonArray(std::move(array));
    }

    UiWidgetAssetParameterType parseParameterType(const std::string& value)
    {
        if (value == "Number") return UiWidgetAssetParameterType::Number;
        if (value == "Bool") return UiWidgetAssetParameterType::Bool;
        if (value == "Color") return UiWidgetAssetParameterType::Color;
        if (value == "Asset") return UiWidgetAssetParameterType::Asset;
        if (value == "BindingPath") return UiWidgetAssetParameterType::BindingPath;
        return UiWidgetAssetParameterType::String;
    }

    std::string parameterTypeString(UiWidgetAssetParameterType value)
    {
        switch (value)
        {
            case UiWidgetAssetParameterType::Number: return "Number";
            case UiWidgetAssetParameterType::Bool: return "Bool";
            case UiWidgetAssetParameterType::Color: return "Color";
            case UiWidgetAssetParameterType::Asset: return "Asset";
            case UiWidgetAssetParameterType::BindingPath: return "BindingPath";
            case UiWidgetAssetParameterType::String:
            default: return "String";
        }
    }

    std::unordered_map<std::string, std::string> parseStringMap(const UiJsonValue* value)
    {
        std::unordered_map<std::string, std::string> result;
        const Object* object = value == nullptr ? nullptr : asObject(*value);
        if (object == nullptr)
            return result;
        for (const auto& [key, member] : *object)
            result[key] = scalarToString(member);
        return result;
    }

    UiJsonValue serializeStringMap(const std::unordered_map<std::string, std::string>& values)
    {
        Object object;
        for (const auto& [key, value] : values)
            object.emplace_back(key, jsonString(value));
        return jsonObject(std::move(object));
    }

    std::unordered_map<std::string, std::vector<UiSerializedWidget>> parseSlotContent(const UiJsonValue* value)
    {
        std::unordered_map<std::string, std::vector<UiSerializedWidget>> result;
        const Object* object = value == nullptr ? nullptr : asObject(*value);
        if (object == nullptr)
            return result;
        for (const auto& [slotName, slotValue] : *object)
        {
            const Array* array = asArray(slotValue);
            if (array == nullptr)
                continue;
            std::vector<UiSerializedWidget>& children = result[slotName];
            for (const UiJsonValue& item : *array)
            {
                UiSerializedWidget child;
                parseUiSerializedWidget(item, child);
                children.push_back(std::move(child));
            }
        }
        return result;
    }

    UiJsonValue serializeSlotContent(
        const std::unordered_map<std::string, std::vector<UiSerializedWidget>>& values)
    {
        Object object;
        for (const auto& [slotName, children] : values)
        {
            Array array;
            for (const UiSerializedWidget& child : children)
                array.push_back(serializeUiSerializedWidget(child));
            object.emplace_back(slotName, jsonArray(std::move(array)));
        }
        return jsonObject(std::move(object));
    }

    UiWidgetAssetParameter parseParameter(const UiJsonValue& json)
    {
        UiWidgetAssetParameter parameter;
        if (!json.isObject())
            return parameter;
        parameter.name = stringOr(json, "name", parameter.name);
        parameter.type = parseParameterType(stringOr(json, "type", "String"));
        if (const UiJsonValue* defaultValue = json.find("default"))
            parameter.defaultValue = scalarToString(*defaultValue);
        parameter.description = stringOr(json, "description", parameter.description);
        parameter.required = boolOr(json, "required", parameter.required);
        return parameter;
    }

    UiJsonValue serializeParameter(const UiWidgetAssetParameter& parameter)
    {
        return jsonObject({
            {"name", jsonString(parameter.name)},
            {"type", jsonString(parameterTypeString(parameter.type))},
            {"default", jsonString(parameter.defaultValue)},
            {"description", jsonString(parameter.description)},
            {"required", jsonBool(parameter.required)}
        });
    }

    UiWidgetAssetSlot parseSlot(const UiJsonValue& json)
    {
        UiWidgetAssetSlot slot;
        if (!json.isObject())
            return slot;
        slot.name = stringOr(json, "name", slot.name);
        slot.target = stringOr(json, "target", slot.target);
        slot.description = stringOr(json, "description", slot.description);
        if (const UiJsonValue* children = json.find("children"))
        {
            if (const Array* array = asArray(*children))
            {
                for (const UiJsonValue& item : *array)
                {
                    UiSerializedWidget child;
                    parseUiSerializedWidget(item, child);
                    slot.defaultChildren.push_back(std::move(child));
                }
            }
        }
        return slot;
    }

    UiJsonValue serializeSlot(const UiWidgetAssetSlot& slot)
    {
        Array children;
        for (const UiSerializedWidget& child : slot.defaultChildren)
            children.push_back(serializeUiSerializedWidget(child));
        return jsonObject({
            {"name", jsonString(slot.name)},
            {"target", jsonString(slot.target)},
            {"description", jsonString(slot.description)},
            {"children", jsonArray(std::move(children))}
        });
    }

    UiWidgetAssetVariant parseVariant(const UiJsonValue& json)
    {
        UiWidgetAssetVariant variant;
        if (!json.isObject())
            return variant;
        variant.name = stringOr(json, "name", variant.name);
        variant.displayName = stringOr(json, "displayName", variant.displayName);
        variant.description = stringOr(json, "description", variant.description);
        variant.tags = parseStringArray(json.find("tags"));
        variant.parameterDefaults = parseStringMap(json.find("parameters"));
        variant.slotDefaults = parseSlotContent(json.find("slots"));
        if (const UiJsonValue* root = json.find("root"))
        {
            parseUiSerializedWidget(*root, variant.rootOverride);
            variant.hasRootOverride = true;
        }
        return variant;
    }

    UiJsonValue serializeVariant(const UiWidgetAssetVariant& variant)
    {
        Object object = {
            {"name", jsonString(variant.name)},
            {"displayName", jsonString(variant.displayName)},
            {"description", jsonString(variant.description)},
            {"tags", serializeStringArray(variant.tags)},
            {"parameters", serializeStringMap(variant.parameterDefaults)},
            {"slots", serializeSlotContent(variant.slotDefaults)}
        };
        if (variant.hasRootOverride)
            object.emplace_back("root", serializeUiSerializedWidget(variant.rootOverride));
        return jsonObject(std::move(object));
    }

    bool hasWidgetDefinition(const UiSerializedWidget& widget)
    {
        return !widget.type.empty() || !widget.asset.empty();
    }

    void appendUnique(std::vector<std::string>& target, const std::vector<std::string>& source)
    {
        for (const std::string& value : source)
        {
            if (std::find(target.begin(), target.end(), value) == target.end())
                target.push_back(value);
        }
    }

    void mergeBindings(std::vector<UiSerializedBinding>& target,
                       const std::vector<UiSerializedBinding>& source)
    {
        for (const UiSerializedBinding& binding : source)
        {
            auto it = std::find_if(target.begin(),
                                   target.end(),
                                   [&binding](const UiSerializedBinding& existing)
                                   {
                                       return existing.property == binding.property;
                                   });
            if (it == target.end())
                target.push_back(binding);
            else
                *it = binding;
        }
    }

    UiSerializedWidget mergeWidget(UiSerializedWidget base, const UiSerializedWidget& overlay)
    {
        if (!hasWidgetDefinition(base))
            return overlay;

        if (!overlay.id.empty()) base.id = overlay.id;
        if (!overlay.type.empty()) base.type = overlay.type;
        if (!overlay.asset.empty()) base.asset = overlay.asset;
        if (!overlay.variant.empty()) base.variant = overlay.variant;
        for (const auto& [name, value] : overlay.parameters)
            base.parameters[name] = value;
        for (const auto& [name, children] : overlay.slots)
            base.slots[name] = children;
        if (overlay.hasLayout)
        {
            base.layout = overlay.layout;
            base.hasLayout = true;
        }
        if (!overlay.styleClass.empty()) base.styleClass = overlay.styleClass;
        if (!overlay.labelStyleClass.empty()) base.labelStyleClass = overlay.labelStyleClass;
        if (!overlay.text.empty()) base.text = overlay.text;
        if (overlay.hasTextKey || !overlay.textKey.empty())
        {
            base.textKey = overlay.textKey;
            base.hasTextKey = true;
        }
        if (!overlay.imageAsset.empty()) base.imageAsset = overlay.imageAsset;
        base.horizontalAlign = overlay.horizontalAlign;
        base.verticalAlign = overlay.verticalAlign;
        base.wrapMode = overlay.wrapMode;
        if (overlay.maxLines != 0) base.maxLines = overlay.maxLines;
        base.imageTint = overlay.imageTint;
        if (overlay.imageAspect != 1.0f) base.imageAspect = overlay.imageAspect;
        if (overlay.rotation != 0.0f) base.rotation = overlay.rotation;
        if (overlay.progressValue != 0.0f) base.progressValue = overlay.progressValue;
        if (overlay.contentOffset.x != 0.0f || overlay.contentOffset.y != 0.0f)
            base.contentOffset = overlay.contentOffset;
        if (overlay.hasVisible)
        {
            base.visible = overlay.visible;
            base.hasVisible = true;
        }
        if (overlay.hasEnabled)
        {
            base.enabled = overlay.enabled;
            base.hasEnabled = true;
        }
        if (overlay.hasInteractable)
        {
            base.interactable = overlay.interactable;
            base.hasInteractable = true;
        }
        if (overlay.hasDecorative)
        {
            base.decorative = overlay.decorative;
            base.hasDecorative = true;
        }
        if (overlay.hasSemantics)
        {
            base.semantics = overlay.semantics;
            base.semanticLocalization = overlay.semanticLocalization;
            base.hasSemantics = true;
        }
        else if (overlay.semanticLocalization.any())
        {
            base.semanticLocalization = overlay.semanticLocalization;
            base.hasSemantics = true;
        }
        if (overlay.hasSemanticRelationships)
        {
            base.semanticRelationships = overlay.semanticRelationships;
            base.hasSemanticRelationships = true;
        }
        if (overlay.hasNavigation)
        {
            base.navigation = overlay.navigation;
            base.hasNavigation = true;
        }
        if (overlay.dragSource) base.dragSource = overlay.dragSource;
        if (overlay.dropTarget) base.dropTarget = overlay.dropTarget;
        if (overlay.stateTransition) base.stateTransition = overlay.stateTransition;
        mergeBindings(base.bindings, overlay.bindings);

        for (const UiSerializedWidget& child : overlay.children)
        {
            auto it = std::find_if(base.children.begin(),
                                   base.children.end(),
                                   [&child](const UiSerializedWidget& existing)
                                   {
                                       return !child.id.empty() && existing.id == child.id;
                                   });
            if (it == base.children.end())
                base.children.push_back(child);
            else
                *it = mergeWidget(*it, child);
        }
        return base;
    }

    UiWidgetAssetDefinition mergeDefinition(UiWidgetAssetDefinition base,
                                            const UiWidgetAssetDefinition& overlay)
    {
        if (!overlay.id.empty()) base.id = overlay.id;
        if (overlay.version != 1) base.version = overlay.version;
        if (!overlay.displayName.empty()) base.displayName = overlay.displayName;
        if (!overlay.description.empty()) base.description = overlay.description;
        appendUnique(base.tags, overlay.tags);
        appendUnique(base.dependencies, overlay.dependencies);
        base.baseAsset = overlay.baseAsset;
        for (const auto& [name, parameter] : overlay.parameters)
            base.parameters[name] = parameter;
        for (const auto& [name, slot] : overlay.slots)
            base.slots[name] = slot;
        for (const auto& [name, variant] : overlay.variants)
            base.variants[name] = variant;
        if (hasWidgetDefinition(overlay.root))
            base.root = mergeWidget(base.root, overlay.root);
        return base;
    }

    void applyVariant(UiWidgetAssetDefinition& definition,
                      const UiWidgetAssetVariant& variant)
    {
        appendUnique(definition.tags, variant.tags);
        for (const auto& [name, value] : variant.parameterDefaults)
        {
            UiWidgetAssetParameter& parameter = definition.parameters[name];
            if (parameter.name.empty())
                parameter.name = name;
            parameter.defaultValue = value;
        }
        for (const auto& [slotName, children] : variant.slotDefaults)
        {
            UiWidgetAssetSlot& slot = definition.slots[slotName];
            if (slot.name.empty())
                slot.name = slotName;
            slot.defaultChildren = children;
        }
        if (variant.hasRootOverride)
            definition.root = mergeWidget(definition.root, variant.rootOverride);
    }

    bool validateParameterValue(const UiWidgetAssetParameter& parameter,
                                const std::string& value)
    {
        if (value.empty())
            return !parameter.required;
        if (parameter.type == UiWidgetAssetParameterType::Bool)
            return value == "true" || value == "false" || value == "0" || value == "1";
        if (parameter.type == UiWidgetAssetParameterType::Number)
        {
            char* end = nullptr;
            std::strtof(value.c_str(), &end);
            return end != value.c_str() && end != nullptr && *end == '\0';
        }
        return true;
    }

    std::string substituteString(const std::string& value,
                                 const std::unordered_map<std::string, std::string>& parameters,
                                 UiSerializedValidationResult& validation,
                                 const std::string& path)
    {
        std::string result;
        size_t cursor = 0;
        while (cursor < value.size())
        {
            const size_t open = value.find("{{", cursor);
            if (open == std::string::npos)
            {
                result.append(value.substr(cursor));
                break;
            }
            result.append(value.substr(cursor, open - cursor));
            const size_t close = value.find("}}", open + 2);
            if (close == std::string::npos)
            {
                result.append(value.substr(open));
                break;
            }
            const std::string key = value.substr(open + 2, close - open - 2);
            const auto it = parameters.find(key);
            if (it == parameters.end())
            {
                validation.error(path, "unknown widget asset parameter '" + key + "'");
            }
            else
            {
                result.append(it->second);
            }
            cursor = close + 2;
        }
        return result;
    }

    void substituteWidget(UiSerializedWidget& widget,
                          const std::unordered_map<std::string, std::string>& parameters,
                          UiSerializedValidationResult& validation,
                          const std::string& path)
    {
        widget.id = substituteString(widget.id, parameters, validation, path + ".id");
        widget.type = substituteString(widget.type, parameters, validation, path + ".type");
        widget.asset = substituteString(widget.asset, parameters, validation, path + ".asset");
        widget.variant = substituteString(widget.variant, parameters, validation, path + ".variant");
        widget.styleClass = substituteString(widget.styleClass, parameters, validation, path + ".styleClass");
        widget.labelStyleClass = substituteString(widget.labelStyleClass, parameters, validation, path + ".labelStyleClass");
        widget.text = substituteString(widget.text, parameters, validation, path + ".text");
        widget.textKey = substituteString(widget.textKey, parameters, validation, path + ".textKey");
        widget.imageAsset = substituteString(widget.imageAsset, parameters, validation, path + ".imageAsset");
        widget.navigation.group = substituteString(widget.navigation.group, parameters, validation, path + ".navigation.group");
        for (auto& [_, target] : widget.navigation.neighbors)
            target = substituteString(target, parameters, validation, path + ".navigation.neighbors");
        if (widget.hasSemantics || widget.semanticLocalization.any())
        {
            widget.semantics.name = substituteString(widget.semantics.name, parameters, validation, path + ".semantics.name");
            widget.semantics.description = substituteString(widget.semantics.description, parameters, validation, path + ".semantics.description");
            widget.semantics.hint = substituteString(widget.semantics.hint, parameters, validation, path + ".semantics.hint");
            widget.semantics.value = substituteString(widget.semantics.value, parameters, validation, path + ".semantics.value");
            widget.semanticLocalization.nameKey =
                substituteString(widget.semanticLocalization.nameKey, parameters, validation, path + ".semantics.nameKey");
            widget.semanticLocalization.descriptionKey =
                substituteString(widget.semanticLocalization.descriptionKey,
                                 parameters,
                                 validation,
                                 path + ".semantics.descriptionKey");
            widget.semanticLocalization.hintKey =
                substituteString(widget.semanticLocalization.hintKey, parameters, validation, path + ".semantics.hintKey");
            widget.semanticLocalization.valueKey =
                substituteString(widget.semanticLocalization.valueKey, parameters, validation, path + ".semantics.valueKey");
        }
        if (widget.hasSemanticRelationships)
        {
            const auto substituteRefs = [&](std::vector<std::string>& refs, const std::string& refPath)
            {
                for (size_t i = 0; i < refs.size(); ++i)
                    refs[i] = substituteString(refs[i], parameters, validation, refPath + "[" + std::to_string(i) + "]");
            };
            substituteRefs(widget.semanticRelationships.labelledBy, path + ".semantics.relationships.labelledBy");
            substituteRefs(widget.semanticRelationships.describedBy, path + ".semantics.relationships.describedBy");
            substituteRefs(widget.semanticRelationships.controls, path + ".semantics.relationships.controls");
            substituteRefs(widget.semanticRelationships.owns, path + ".semantics.relationships.owns");
            widget.semanticRelationships.activeDescendant =
                substituteString(widget.semanticRelationships.activeDescendant,
                                 parameters,
                                 validation,
                                 path + ".semantics.relationships.activeDescendant");
            widget.semanticRelationships.popup = substituteString(widget.semanticRelationships.popup,
                                                                  parameters,
                                                                  validation,
                                                                  path + ".semantics.relationships.popup");
            widget.semanticRelationships.tooltip = substituteString(widget.semanticRelationships.tooltip,
                                                                    parameters,
                                                                    validation,
                                                                    path + ".semantics.relationships.tooltip");
        }
        if (widget.dragSource)
        {
            widget.dragSource->payloadType = substituteString(widget.dragSource->payloadType, parameters, validation, path + ".dragSource.payloadType");
            widget.dragSource->payloadLabel = substituteString(widget.dragSource->payloadLabel, parameters, validation, path + ".dragSource.payloadLabel");
        }
        if (widget.dropTarget)
        {
            for (std::string& type : widget.dropTarget->acceptedPayloadTypes)
                type = substituteString(type, parameters, validation, path + ".dropTarget.acceptedPayloadTypes");
        }
        for (UiSerializedBinding& binding : widget.bindings)
        {
            binding.path = substituteString(binding.path, parameters, validation, path + ".binding.path");
            binding.formatter = substituteString(binding.formatter, parameters, validation, path + ".binding.formatter");
            binding.transform = substituteString(binding.transform, parameters, validation, path + ".binding.transform");
        }
        for (auto& [name, value] : widget.parameters)
            value = substituteString(value, parameters, validation, path + ".parameters." + name);
        for (size_t i = 0; i < widget.children.size(); ++i)
            substituteWidget(widget.children[i], parameters, validation, path + ".children[" + std::to_string(i) + "]");
        for (auto& [slotName, children] : widget.slots)
        {
            for (size_t i = 0; i < children.size(); ++i)
                substituteWidget(children[i], parameters, validation, path + ".slots." + slotName + "[" + std::to_string(i) + "]");
        }
    }

    bool insertSlotChildren(UiSerializedWidget& widget,
                            const std::string& target,
                            const std::vector<UiSerializedWidget>& children)
    {
        if (target.empty() || widget.id == target)
        {
            widget.children.insert(widget.children.end(), children.begin(), children.end());
            return true;
        }
        for (UiSerializedWidget& child : widget.children)
        {
            if (insertSlotChildren(child, target, children))
                return true;
        }
        return false;
    }

    std::string prefixedId(const std::string& id,
                           const std::string& rootId,
                           const std::string& prefix)
    {
        if (prefix.empty() || id.empty())
            return id;
        if (id == rootId)
            return prefix;
        return prefix + "." + id;
    }

    void prefixWidgetIds(UiSerializedWidget& widget,
                         const std::string& rootId,
                         const std::string& prefix)
    {
        if (prefix.empty())
            return;
        widget.id = prefixedId(widget.id, rootId, prefix);
        for (auto& [_, target] : widget.navigation.neighbors)
            target = prefixedId(target, rootId, prefix);
        for (UiSerializedWidget& child : widget.children)
            prefixWidgetIds(child, rootId, prefix);
        for (auto& [_, children] : widget.slots)
        {
            for (UiSerializedWidget& child : children)
                prefixWidgetIds(child, rootId, prefix);
        }
    }

    void applyReferenceOverrides(UiSerializedWidget& target, const UiSerializedWidget& reference)
    {
        if (reference.hasLayout)
        {
            target.layout = reference.layout;
            target.hasLayout = true;
        }
        if (!reference.styleClass.empty()) target.styleClass = reference.styleClass;
        if (!reference.labelStyleClass.empty()) target.labelStyleClass = reference.labelStyleClass;
        if (!reference.text.empty()) target.text = reference.text;
        if (reference.hasTextKey || !reference.textKey.empty())
        {
            target.textKey = reference.textKey;
            target.hasTextKey = true;
        }
        if (!reference.imageAsset.empty()) target.imageAsset = reference.imageAsset;
        if (reference.hasVisible)
        {
            target.visible = reference.visible;
            target.hasVisible = true;
        }
        if (reference.hasEnabled)
        {
            target.enabled = reference.enabled;
            target.hasEnabled = true;
        }
        if (reference.hasInteractable)
        {
            target.interactable = reference.interactable;
            target.hasInteractable = true;
        }
        if (reference.hasDecorative)
        {
            target.decorative = reference.decorative;
            target.hasDecorative = true;
        }
        if (reference.hasSemantics)
        {
            target.semantics = reference.semantics;
            target.semanticLocalization = reference.semanticLocalization;
            target.hasSemantics = true;
        }
        else if (reference.semanticLocalization.any())
        {
            target.semanticLocalization = reference.semanticLocalization;
            target.hasSemantics = true;
        }
        if (reference.hasSemanticRelationships)
        {
            target.semanticRelationships = reference.semanticRelationships;
            target.hasSemanticRelationships = true;
        }
        if (reference.hasNavigation)
        {
            target.navigation = reference.navigation;
            target.hasNavigation = true;
        }
        if (reference.dragSource) target.dragSource = reference.dragSource;
        if (reference.dropTarget) target.dropTarget = reference.dropTarget;
        if (reference.stateTransition) target.stateTransition = reference.stateTransition;
        mergeBindings(target.bindings, reference.bindings);
    }

    std::unordered_map<std::string, std::string> collectParameterValues(
        const UiWidgetAssetDefinition& definition,
        const UiWidgetAssetInstanceDesc& desc,
        UiSerializedValidationResult& validation)
    {
        std::unordered_map<std::string, std::string> values;
        for (const auto& [name, parameter] : definition.parameters)
            values[name] = parameter.defaultValue;
        for (const auto& [name, value] : desc.parameterOverrides)
            values[name] = value;

        for (const auto& [name, parameter] : definition.parameters)
        {
            const auto it = values.find(name);
            if (it == values.end() || it->second.empty())
            {
                if (parameter.required)
                    validation.error("$.parameters." + name, "required widget asset parameter is missing");
                continue;
            }
            if (!validateParameterValue(parameter, it->second))
                validation.error("$.parameters." + name, "widget asset parameter value has the wrong type");
        }
        return values;
    }

    UiJsonValue serializeAssetJson(const UiWidgetAssetDefinition& asset)
    {
        Array tags;
        for (const std::string& tag : asset.tags)
            tags.push_back(jsonString(tag));
        Array dependencies;
        for (const std::string& dependency : asset.dependencies)
            dependencies.push_back(jsonString(dependency));
        Array parameters;
        for (const auto& [_, parameter] : asset.parameters)
            parameters.push_back(serializeParameter(parameter));
        Array slots;
        for (const auto& [_, slot] : asset.slots)
            slots.push_back(serializeSlot(slot));
        Array variants;
        for (const auto& [_, variant] : asset.variants)
            variants.push_back(serializeVariant(variant));

        return jsonObject({
            {"schema", jsonNumber(asset.schemaVersion)},
            {"id", jsonString(asset.id)},
            {"version", jsonNumber(asset.version)},
            {"displayName", jsonString(asset.displayName)},
            {"description", jsonString(asset.description)},
            {"tags", jsonArray(std::move(tags))},
            {"dependencies", jsonArray(std::move(dependencies))},
            {"base", jsonString(asset.baseAsset)},
            {"parameters", jsonArray(std::move(parameters))},
            {"slots", jsonArray(std::move(slots))},
            {"variants", jsonArray(std::move(variants))},
            {"root", serializeUiSerializedWidget(asset.root)}
        });
    }
}

bool parseUiWidgetAssetDefinition(const std::string& json,
                                  UiWidgetAssetDefinition& outAsset,
                                  UiSerializedValidationResult* outValidation)
{
    UiSerializedValidationResult validation;
    UiJsonValue root;
    std::string error;
    if (!parseUiJson(json, root, &error))
    {
        validation.error("$", error);
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }
    if (!root.isObject())
    {
        validation.error("$", "widget asset root must be an object");
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    UiWidgetAssetDefinition asset;
    asset.schemaVersion = intOr(root, "schema", asset.schemaVersion);
    asset.id = stringOr(root, "id", asset.id);
    asset.version = intOr(root, "version", asset.version);
    asset.displayName = stringOr(root, "displayName", asset.displayName);
    asset.description = stringOr(root, "description", asset.description);
    asset.tags = parseStringArray(root.find("tags"));
    asset.dependencies = parseStringArray(root.find("dependencies"));
    asset.baseAsset = stringOr(root, "base", stringOr(root, "baseAsset", asset.baseAsset));

    if (const UiJsonValue* parameters = root.find("parameters"))
    {
        if (const Array* array = asArray(*parameters))
        {
            for (const UiJsonValue& item : *array)
            {
                UiWidgetAssetParameter parameter = parseParameter(item);
                if (!parameter.name.empty())
                    asset.parameters[parameter.name] = std::move(parameter);
            }
        }
        else if (const Object* object = asObject(*parameters))
        {
            for (const auto& [name, value] : *object)
            {
                UiWidgetAssetParameter parameter;
                parameter.name = name;
                parameter.defaultValue = scalarToString(value);
                asset.parameters[name] = std::move(parameter);
            }
        }
    }

    if (const UiJsonValue* slots = root.find("slots"))
    {
        if (const Array* array = asArray(*slots))
        {
            for (const UiJsonValue& item : *array)
            {
                UiWidgetAssetSlot slot = parseSlot(item);
                if (!slot.name.empty())
                    asset.slots[slot.name] = std::move(slot);
            }
        }
    }

    if (const UiJsonValue* variants = root.find("variants"))
    {
        if (const Array* array = asArray(*variants))
        {
            for (const UiJsonValue& item : *array)
            {
                UiWidgetAssetVariant variant = parseVariant(item);
                if (!variant.name.empty())
                    asset.variants[variant.name] = std::move(variant);
            }
        }
    }

    if (const UiJsonValue* widget = root.find("root"))
        parseUiSerializedWidget(*widget, asset.root);
    else if (asset.baseAsset.empty())
        validation.error("$.root", "root widget is required for non-derived widget assets");

    if (asset.schemaVersion < 1)
    {
        asset.schemaVersion = 1;
        validation.warning("$.schema", "migrated widget asset schema to version 1");
    }
    else if (asset.schemaVersion > UI_WIDGET_ASSET_SCHEMA_VERSION)
    {
        validation.error("$.schema", "unsupported future widget asset schema");
    }
    if (asset.id.empty())
        validation.error("$.id", "widget asset id is required");

    outAsset = std::move(asset);
    if (outValidation != nullptr)
        *outValidation = validation;
    return validation.valid();
}

std::string serializeUiWidgetAssetDefinition(const UiWidgetAssetDefinition& asset)
{
    return serializeUiJson(serializeAssetJson(asset), 0);
}

bool loadUiWidgetAssetFromFile(const std::string& path,
                               UiWidgetAssetDefinition& outAsset,
                               UiSerializedValidationResult* outValidation)
{
    std::ifstream file(path);
    if (!file)
    {
        if (outValidation != nullptr)
            outValidation->error(path, "failed to open widget asset file");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseUiWidgetAssetDefinition(buffer.str(), outAsset, outValidation);
}

bool saveUiWidgetAssetToFile(const std::string& path,
                             const UiWidgetAssetDefinition& asset,
                             std::string* outError)
{
    std::ofstream file(path);
    if (!file)
    {
        if (outError != nullptr)
            *outError = "failed to open widget asset file for writing";
        return false;
    }
    file << serializeUiWidgetAssetDefinition(asset);
    return true;
}

bool UiWidgetAssetRegistry::registerAsset(const UiWidgetAssetDefinition& asset,
                                          UiSerializedValidationResult* outValidation)
{
    UiSerializedValidationResult validation = validateAsset(asset);
    if (outValidation != nullptr)
        *outValidation = validation;
    if (!validation.valid())
        return false;

    assets[asset.id] = asset;
    return true;
}

bool UiWidgetAssetRegistry::unregisterAsset(const std::string& assetId)
{
    return assets.erase(assetId) > 0;
}

void UiWidgetAssetRegistry::clear()
{
    assets.clear();
}

const UiWidgetAssetDefinition* UiWidgetAssetRegistry::findAsset(const std::string& assetId) const
{
    const auto it = assets.find(assetId);
    return it == assets.end() ? nullptr : &it->second;
}

std::vector<std::string> UiWidgetAssetRegistry::assetIds() const
{
    std::vector<std::string> ids;
    ids.reserve(assets.size());
    for (const auto& [id, _] : assets)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

UiSerializedValidationResult UiWidgetAssetRegistry::validateAsset(const UiWidgetAssetDefinition& asset) const
{
    UiSerializedValidationResult validation;
    if (asset.schemaVersion != UI_WIDGET_ASSET_SCHEMA_VERSION)
        validation.error("$.schema", "widget asset schema version is not supported");
    if (asset.id.empty())
        validation.error("$.id", "widget asset id is required");
    if (!asset.baseAsset.empty() && findAsset(asset.baseAsset) == nullptr)
        validation.error("$.base", "base widget asset '" + asset.baseAsset + "' is not registered");
    if (asset.baseAsset.empty() && !hasWidgetDefinition(asset.root))
        validation.error("$.root", "root widget is required");
    for (const auto& [name, parameter] : asset.parameters)
    {
        if (parameter.name.empty() || parameter.name != name)
            validation.error("$.parameters." + name, "parameter name must match its key");
        if (!validateParameterValue(parameter, parameter.defaultValue))
            validation.error("$.parameters." + name, "default parameter value has the wrong type");
    }
    for (const auto& [name, slot] : asset.slots)
    {
        if (slot.name.empty() || slot.name != name)
            validation.error("$.slots." + name, "slot name must match its key");
        if (slot.target.empty())
            validation.error("$.slots." + name, "slot target widget id is required");
    }
    for (const auto& [name, variant] : asset.variants)
    {
        if (variant.name.empty() || variant.name != name)
            validation.error("$.variants." + name, "variant name must match its key");
    }
    if (hasWidgetDefinition(asset.root))
    {
        UiSerializedAsset serialized;
        serialized.id = asset.id;
        serialized.root = asset.root;
        UiSerializedValidationResult serializedValidation =
            validateUiSerializedAsset(serialized, nullptr, this);
        validation.issues.insert(validation.issues.end(),
                                 serializedValidation.issues.begin(),
                                 serializedValidation.issues.end());
    }
    return validation;
}

UiWidgetAssetResolveResult UiWidgetAssetRegistry::resolveAsset(const std::string& assetId,
                                                               const std::string& variant) const
{
    std::vector<std::string> stack;
    return resolveAssetRecursive(assetId, variant, stack);
}

UiWidgetAssetResolveResult UiWidgetAssetRegistry::resolveAssetRecursive(
    const std::string& assetId,
    const std::string& variant,
    std::vector<std::string>& stack) const
{
    UiWidgetAssetResolveResult result;
    const UiWidgetAssetDefinition* asset = findAsset(assetId);
    if (asset == nullptr)
    {
        result.validation.error("$.asset", "unknown widget asset '" + assetId + "'");
        return result;
    }
    if (std::find(stack.begin(), stack.end(), assetId) != stack.end())
    {
        result.validation.error("$.asset", "cyclic widget asset inheritance or dependency involving '" + assetId + "'");
        return result;
    }

    stack.push_back(assetId);
    UiWidgetAssetDefinition resolved = *asset;
    if (!asset->baseAsset.empty())
    {
        UiWidgetAssetResolveResult base = resolveAssetRecursive(asset->baseAsset, {}, stack);
        result.validation.issues.insert(result.validation.issues.end(),
                                        base.validation.issues.begin(),
                                        base.validation.issues.end());
        if (!base.success)
        {
            stack.pop_back();
            return result;
        }
        resolved = mergeDefinition(base.definition, *asset);
    }

    if (!variant.empty())
    {
        const auto it = resolved.variants.find(variant);
        if (it == resolved.variants.end())
            result.validation.error("$.variant", "unknown widget asset variant '" + variant + "'");
        else
            applyVariant(resolved, it->second);
    }

    stack.pop_back();
    result.definition = std::move(resolved);
    result.success = result.validation.valid();
    return result;
}

UiWidgetAssetExpandResult UiWidgetAssetRegistry::expandAsset(const UiWidgetAssetInstanceDesc& desc) const
{
    std::vector<std::string> stack;
    return expandAssetRecursive(desc, stack);
}

UiWidgetAssetExpandResult UiWidgetAssetRegistry::expandAssetRecursive(
    const UiWidgetAssetInstanceDesc& desc,
    std::vector<std::string>& stack) const
{
    UiWidgetAssetExpandResult result;
    if (std::find(stack.begin(), stack.end(), desc.assetId) != stack.end())
    {
        result.validation.error("$.asset", "cyclic widget asset reference involving '" + desc.assetId + "'");
        return result;
    }

    stack.push_back(desc.assetId);
    std::vector<std::string> inheritanceStack;
    UiWidgetAssetResolveResult resolved = resolveAssetRecursive(desc.assetId, desc.variant, inheritanceStack);
    result.validation.issues.insert(result.validation.issues.end(),
                                    resolved.validation.issues.begin(),
                                    resolved.validation.issues.end());
    if (!resolved.success)
    {
        stack.pop_back();
        return result;
    }

    std::unordered_map<std::string, std::string> parameters =
        collectParameterValues(resolved.definition, desc, result.validation);

    UiSerializedWidget root = resolved.definition.root;
    for (const auto& [slotName, slot] : resolved.definition.slots)
    {
        auto overrideIt = desc.slotContent.find(slotName);
        const std::vector<UiSerializedWidget>& children =
            overrideIt == desc.slotContent.end() ? slot.defaultChildren : overrideIt->second;
        if (!children.empty() && !insertSlotChildren(root, slot.target, children))
            result.validation.error("$.slots." + slotName, "slot target '" + slot.target + "' was not found");
    }

    substituteWidget(root, parameters, result.validation, "$.root");
    const std::string rootId = root.id;
    prefixWidgetIds(root, rootId, desc.idPrefix);
    if (!expandSerializedWidgetRecursive(root, result.widget, result.validation, stack))
    {
        stack.pop_back();
        return result;
    }

    stack.pop_back();
    result.success = result.validation.valid();
    return result;
}

bool UiWidgetAssetRegistry::expandSerializedWidget(const UiSerializedWidget& widget,
                                                   UiSerializedWidget& outWidget,
                                                   UiSerializedValidationResult* outValidation) const
{
    UiSerializedValidationResult validation;
    std::vector<std::string> stack;
    const bool success = expandSerializedWidgetRecursive(widget, outWidget, validation, stack);
    if (outValidation != nullptr)
        *outValidation = validation;
    return success && validation.valid();
}

bool UiWidgetAssetRegistry::expandSerializedAsset(const UiSerializedAsset& asset,
                                                  UiSerializedAsset& outAsset,
                                                  UiSerializedValidationResult* outValidation) const
{
    UiSerializedValidationResult validation;
    outAsset = asset;
    std::vector<std::string> stack;
    const bool success = expandSerializedWidgetRecursive(asset.root, outAsset.root, validation, stack);
    if (outValidation != nullptr)
        *outValidation = validation;
    return success && validation.valid();
}

bool UiWidgetAssetRegistry::expandSerializedWidgetRecursive(
    const UiSerializedWidget& widget,
    UiSerializedWidget& outWidget,
    UiSerializedValidationResult& validation,
    std::vector<std::string>& stack) const
{
    if (!widget.asset.empty())
    {
        UiWidgetAssetInstanceDesc desc;
        desc.assetId = widget.asset;
        desc.variant = widget.variant;
        desc.idPrefix = widget.id;
        desc.parameterOverrides = widget.parameters;
        desc.slotContent = widget.slots;
        UiWidgetAssetExpandResult expanded = expandAssetRecursive(desc, stack);
        validation.issues.insert(validation.issues.end(),
                                 expanded.validation.issues.begin(),
                                 expanded.validation.issues.end());
        if (!expanded.success)
            return false;

        outWidget = std::move(expanded.widget);
        applyReferenceOverrides(outWidget, widget);
        outWidget.children.insert(outWidget.children.end(), widget.children.begin(), widget.children.end());
        return validation.valid();
    }

    outWidget = widget;
    for (UiSerializedWidget& child : outWidget.children)
    {
        UiSerializedWidget expandedChild;
        if (!expandSerializedWidgetRecursive(child, expandedChild, validation, stack))
            return false;
        child = std::move(expandedChild);
    }
    for (auto& [slotName, children] : outWidget.slots)
    {
        for (UiSerializedWidget& child : children)
        {
            UiSerializedWidget expandedChild;
            if (!expandSerializedWidgetRecursive(child, expandedChild, validation, stack))
                return false;
            child = std::move(expandedChild);
        }
        (void)slotName;
    }
    return validation.valid();
}

UiSerializedLoadResult UiWidgetAssetRegistry::instantiate(
    UiSystem& ui,
    UiSurfaceId surface,
    UiMountId mount,
    const UiWidgetAssetInstanceDesc& desc,
    const IUiSerializedBindingResolver* bindingResolver,
    const UiTheme* validationTheme) const
{
    UiSerializedLoadResult result;
    UiWidgetAssetExpandResult expanded = expandAsset(desc);
    result.validation.issues.insert(result.validation.issues.end(),
                                    expanded.validation.issues.begin(),
                                    expanded.validation.issues.end());
    if (!expanded.success)
        return result;

    UiSerializedAsset asset;
    asset.id = desc.assetId;
    asset.root = std::move(expanded.widget);
    result = UiSerializationRuntime::instantiate(ui,
                                                 surface,
                                                 mount,
                                                 asset,
                                                 bindingResolver,
                                                 validationTheme,
                                                 this);
    return result;
}
