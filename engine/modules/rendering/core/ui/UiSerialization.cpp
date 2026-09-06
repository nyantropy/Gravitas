#include "UiSerialization.h"
#include "GtsJsonParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "UiSystem.h"
#include "UiWidget.h"
#include "UiWidgetAsset.h"

namespace
{
    using Object = GtsJsonValue::Object;
    using Array = GtsJsonValue::Array;

    UiVec2 parseVec2(const GtsJsonValue& value, UiVec2 fallback)
    {
        const Array* array = value.tryArray();
        if (array == nullptr || array->size() < 2)
            return fallback;

        const auto x = (*array)[0].tryFloat();
        const auto y = (*array)[1].tryFloat();
        return x && y ? UiVec2{*x, *y} : fallback;
    }

    UiRect parseRect(const GtsJsonValue& value, UiRect fallback)
    {
        const Array* array = value.tryArray();
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto x = (*array)[0].tryFloat();
        const auto y = (*array)[1].tryFloat();
        const auto w = (*array)[2].tryFloat();
        const auto h = (*array)[3].tryFloat();
        return x && y && w && h ? UiRect{*x, *y, *w, *h} : fallback;
    }

    UiThickness parseThickness(const GtsJsonValue& value, UiThickness fallback)
    {
        const Array* array = value.tryArray();
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto l = (*array)[0].tryFloat();
        const auto t = (*array)[1].tryFloat();
        const auto r = (*array)[2].tryFloat();
        const auto b = (*array)[3].tryFloat();
        return l && t && r && b ? UiThickness{*l, *t, *r, *b} : fallback;
    }

    UiColor parseColor(const GtsJsonValue& value, UiColor fallback)
    {
        const Array* array = value.tryArray();
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto r = (*array)[0].tryFloat();
        const auto g = (*array)[1].tryFloat();
        const auto b = (*array)[2].tryFloat();
        const auto a = (*array)[3].tryFloat();
        return r && g && b && a ? UiColor{*r, *g, *b, *a} : fallback;
    }

    UiLayoutLength parseLength(const GtsJsonValue& value, UiLayoutLength fallback)
    {
        const Object* object = value.tryObject();
        if (object == nullptr)
            return fallback;

        UiLayoutLength length = fallback;
        if (const auto unit = value.findString("unit"))
            length.unit = gts::enumValue(uiLayoutUnitNames, *unit).value_or(length.unit);
        if (const auto amount = value.findFloat("value"))
            length.value = *amount;
        return length;
    }

    GtsJsonValue serializeLength(const UiLayoutLength& length)
    {
        return GtsJsonValue::Object({
            {"unit", std::string(gts::enumName(uiLayoutUnitNames, length.unit).value_or("Auto"))},
            {"value", length.value}
        });
    }

    void parseLayoutConstraints(const GtsJsonValue& json, UiLayoutConstraints& constraints)
    {
        if (const GtsJsonValue* value = json.find("minWidth")) constraints.minWidth = parseLength(*value, constraints.minWidth);
        if (const GtsJsonValue* value = json.find("minHeight")) constraints.minHeight = parseLength(*value, constraints.minHeight);
        if (const GtsJsonValue* value = json.find("maxWidth")) constraints.maxWidth = parseLength(*value, constraints.maxWidth);
        if (const GtsJsonValue* value = json.find("maxHeight")) constraints.maxHeight = parseLength(*value, constraints.maxHeight);
        if (const GtsJsonValue* value = json.find("preferredWidth")) constraints.preferredWidth = parseLength(*value, constraints.preferredWidth);
        if (const GtsJsonValue* value = json.find("preferredHeight")) constraints.preferredHeight = parseLength(*value, constraints.preferredHeight);
        constraints.grow = json.findFloat("grow").value_or(constraints.grow);
        constraints.shrink = json.findFloat("shrink").value_or(constraints.shrink);
        constraints.aspectRatio = json.findFloat("aspectRatio").value_or(constraints.aspectRatio);
        if (const auto value = json.findString("horizontalAlignment"))
            constraints.horizontalAlignment =
                gts::enumValue(uiLayoutAlignmentNames, *value).value_or(constraints.horizontalAlignment);
        if (const auto value = json.findString("verticalAlignment"))
            constraints.verticalAlignment =
                gts::enumValue(uiLayoutAlignmentNames, *value).value_or(constraints.verticalAlignment);
    }

    GtsJsonValue serializeConstraints(const UiLayoutConstraints& constraints)
    {
        return GtsJsonValue::Object(
            {{"minWidth", serializeLength(constraints.minWidth)},
             {"minHeight", serializeLength(constraints.minHeight)},
             {"maxWidth", serializeLength(constraints.maxWidth)},
             {"maxHeight", serializeLength(constraints.maxHeight)},
             {"preferredWidth", serializeLength(constraints.preferredWidth)},
             {"preferredHeight", serializeLength(constraints.preferredHeight)},
             {"grow", constraints.grow},
             {"shrink", constraints.shrink},
             {"aspectRatio", constraints.aspectRatio},
             {"horizontalAlignment",
              std::string(
                  gts::enumName(uiLayoutAlignmentNames, constraints.horizontalAlignment).value_or("Stretch"))},
             {"verticalAlignment",
              std::string(
                  gts::enumName(uiLayoutAlignmentNames, constraints.verticalAlignment).value_or("Stretch"))}});
    }

    UiLayoutSpec parseLayout(const GtsJsonValue& json, UiLayoutSpec layout = {})
    {
        if (!json.isObject())
            return layout;

        if (const auto value = json.findString("mode"))
            layout.layoutMode = gts::enumValue(uiLayoutModeNames, *value).value_or(layout.layoutMode);
        if (const auto value = json.findString("position"))
            layout.positionMode = gts::enumValue(uiPositionModeNames, *value).value_or(layout.positionMode);
        if (const auto value = json.findString("width"))
            layout.widthMode = gts::enumValue(uiSizeModeNames, *value).value_or(layout.widthMode);
        if (const auto value = json.findString("height"))
            layout.heightMode = gts::enumValue(uiSizeModeNames, *value).value_or(layout.heightMode);
        if (const GtsJsonValue* value = json.find("anchorMin")) layout.anchorMin = parseVec2(*value, layout.anchorMin);
        if (const GtsJsonValue* value = json.find("anchorMax")) layout.anchorMax = parseVec2(*value, layout.anchorMax);
        if (const GtsJsonValue* value = json.find("offsetMin")) layout.offsetMin = parseVec2(*value, layout.offsetMin);
        if (const GtsJsonValue* value = json.find("offsetMax")) layout.offsetMax = parseVec2(*value, layout.offsetMax);
        if (const GtsJsonValue* value = json.find("fixedSize"))
        {
            const UiVec2 size = parseVec2(*value, {layout.fixedWidth, layout.fixedHeight});
            layout.fixedWidth = size.x;
            layout.fixedHeight = size.y;
        }
        layout.fixedWidth = json.findFloat("fixedWidth").value_or(layout.fixedWidth);
        layout.fixedHeight = json.findFloat("fixedHeight").value_or(layout.fixedHeight);
        if (const GtsJsonValue* value = json.find("margin")) layout.margin = parseThickness(*value, layout.margin);
        if (const GtsJsonValue* value = json.find("padding")) layout.padding = parseThickness(*value, layout.padding);
        if (const auto value = json.findString("clip"))
            layout.clipMode = gts::enumValue(uiClipModeNames, *value).value_or(layout.clipMode);
        if (const GtsJsonValue* value = json.find("contentOffset")) layout.contentOffset = parseVec2(*value, layout.contentOffset);
        layout.gap = json.findFloat("gap").value_or(layout.gap);
        if (const auto value = json.findString("stackAxis"))
            layout.stackAxis = gts::enumValue(uiLayoutAxisNames, *value).value_or(layout.stackAxis);
        if (const auto value = json.findString("mainAxisAlignment"))
            layout.mainAxisAlignment =
                gts::enumValue(uiLayoutAlignmentNames, *value).value_or(layout.mainAxisAlignment);
        if (const auto value = json.findString("crossAxisAlignment"))
            layout.crossAxisAlignment =
                gts::enumValue(uiLayoutAlignmentNames, *value).value_or(layout.crossAxisAlignment);
        layout.gridColumns = json.findInt32("gridColumns").value_or(layout.gridColumns);
        layout.gridRows = json.findInt32("gridRows").value_or(layout.gridRows);
        layout.gridColumnGap = json.findFloat("gridColumnGap").value_or(layout.gridColumnGap);
        layout.gridRowGap = json.findFloat("gridRowGap").value_or(layout.gridRowGap);
        layout.gridColumn = json.findInt32("gridColumn").value_or(layout.gridColumn);
        layout.gridRow = json.findInt32("gridRow").value_or(layout.gridRow);
        layout.gridColumnSpan = json.findInt32("gridColumnSpan").value_or(layout.gridColumnSpan);
        layout.gridRowSpan = json.findInt32("gridRowSpan").value_or(layout.gridRowSpan);
        if (const auto value = json.findString("dock"))
            layout.dock = gts::enumValue(uiDockEdgeNames, *value).value_or(layout.dock);
        if (const GtsJsonValue* value = json.find("constraints"))
            parseLayoutConstraints(*value, layout.constraints);
        return layout;
    }

    GtsJsonValue serializeLayout(const UiLayoutSpec& layout)
    {
        return GtsJsonValue::Object(
            {{"mode",
              std::string(gts::enumName(uiLayoutModeNames, layout.layoutMode).value_or("Canvas"))},
             {"position",
              std::string(gts::enumName(uiPositionModeNames, layout.positionMode).value_or("Absolute"))},
             {"width", std::string(gts::enumName(uiSizeModeNames, layout.widthMode).value_or("Fixed"))},
             {"height", std::string(gts::enumName(uiSizeModeNames, layout.heightMode).value_or("Fixed"))},
             {"anchorMin", Array{layout.anchorMin.x, layout.anchorMin.y}},
             {"anchorMax", Array{layout.anchorMax.x, layout.anchorMax.y}},
             {"offsetMin", Array{layout.offsetMin.x, layout.offsetMin.y}},
             {"offsetMax", Array{layout.offsetMax.x, layout.offsetMax.y}},
             {"fixedSize", Array{layout.fixedWidth, layout.fixedHeight}},
             {"margin", Array{layout.margin.left, layout.margin.top, layout.margin.right, layout.margin.bottom}},
             {"padding", Array{layout.padding.left, layout.padding.top, layout.padding.right, layout.padding.bottom}},
             {"clip", std::string(gts::enumName(uiClipModeNames, layout.clipMode).value_or("None"))},
             {"contentOffset", Array{layout.contentOffset.x, layout.contentOffset.y}},
             {"gap", layout.gap},
             {"stackAxis",
              std::string(gts::enumName(uiLayoutAxisNames, layout.stackAxis).value_or("Vertical"))},
             {"mainAxisAlignment",
                  std::string(gts::enumName(uiLayoutAlignmentNames, layout.mainAxisAlignment).value_or("Start"))},
             {"crossAxisAlignment",
                  std::string(gts::enumName(uiLayoutAlignmentNames, layout.crossAxisAlignment).value_or("Stretch"))},
             {"gridColumns", layout.gridColumns},
             {"gridRows", layout.gridRows},
             {"gridColumnGap", layout.gridColumnGap},
             {"gridRowGap", layout.gridRowGap},
             {"gridColumn", layout.gridColumn},
             {"gridRow", layout.gridRow},
             {"gridColumnSpan", layout.gridColumnSpan},
             {"gridRowSpan", layout.gridRowSpan},
             {"dock", std::string(gts::enumName(uiDockEdgeNames, layout.dock).value_or("Fill"))},
             {"constraints", serializeConstraints(layout.constraints)}});
    }

    UiAnimationTiming parseTiming(const GtsJsonValue& json, UiAnimationTiming timing = {})
    {
        if (!json.isObject())
            return timing;
        timing.durationSeconds = json.findFloat("duration").value_or(timing.durationSeconds);
        timing.delaySeconds = json.findFloat("delay").value_or(timing.delaySeconds);
        if (const auto value = json.findString("ease"))
            timing.ease = gts::enumValue(gts::tween::tweenEaseNames, *value).value_or(timing.ease);
        timing.repeatCount = json.findUInt32("repeat").value_or(timing.repeatCount);
        timing.loop = json.findBool("loop").value_or(timing.loop);
        timing.pingPong = json.findBool("pingPong").value_or(timing.pingPong);
        timing.snapToEnd = json.findBool("snapToEnd").value_or(timing.snapToEnd);
        return timing;
    }

    GtsJsonValue serializeTiming(const UiAnimationTiming& timing)
    {
        return GtsJsonValue::Object(
            {{"duration", timing.durationSeconds},
             {"delay", timing.delaySeconds},
             {"ease",
              std::string(gts::enumName(gts::tween::tweenEaseNames, timing.ease).value_or("SmoothStep"))},
             {"repeat", timing.repeatCount},
             {"loop", timing.loop},
             {"pingPong", timing.pingPong},
             {"snapToEnd", timing.snapToEnd}});
    }

    UiSemanticRelationship parseRelationship(const GtsJsonValue& json)
    {
        UiSemanticRelationship relationship;
        const auto parseHandleIds = [&](const char* key, std::vector<UiHandle>& target)
        {
            const Array* array = json.findArray(key);
            if (array == nullptr)
                return;
            for (const GtsJsonValue& item : *array)
            {
                if (const auto handle = item.tryUInt32())
                    target.push_back(*handle);
            }
        };
        parseHandleIds("labelledBy", relationship.labelledBy);
        parseHandleIds("describedBy", relationship.describedBy);
        parseHandleIds("controls", relationship.controls);
        parseHandleIds("owns", relationship.owns);
        relationship.activeDescendant = json.findUInt32("activeDescendant").value_or(relationship.activeDescendant);
        relationship.popup = json.findUInt32("popup").value_or(relationship.popup);
        relationship.tooltip = json.findUInt32("tooltip").value_or(relationship.tooltip);
        return relationship;
    }

    UiSerializedSemanticRelationships parseRelationshipIds(const GtsJsonValue& json)
    {
        UiSerializedSemanticRelationships relationships;
        if (!json.isObject())
            return relationships;

        const auto parseIds = [&](const char* key, std::vector<std::string>& target)
        {
            const Array* array = json.findArray(key);
            if (array == nullptr)
                return;

            for (const GtsJsonValue& item : *array)
            {
                if (const auto string = item.tryString())
                    target.push_back(*string);
            }
        };

        parseIds("labelledBy", relationships.labelledBy);
        parseIds("describedBy", relationships.describedBy);
        parseIds("controls", relationships.controls);
        parseIds("owns", relationships.owns);
        relationships.activeDescendant = json.findString("activeDescendant").value_or(relationships.activeDescendant);
        relationships.popup = json.findString("popup").value_or(relationships.popup);
        relationships.tooltip = json.findString("tooltip").value_or(relationships.tooltip);
        return relationships;
    }

    bool hasRelationshipIds(const UiSerializedSemanticRelationships& relationships)
    {
        return !relationships.labelledBy.empty()
            || !relationships.describedBy.empty()
            || !relationships.controls.empty()
            || !relationships.owns.empty()
            || !relationships.activeDescendant.empty()
            || !relationships.popup.empty()
            || !relationships.tooltip.empty();
    }

    UiSerializedSemanticRelationships parseSemanticRelationshipIds(const GtsJsonValue& json)
    {
        if (!json.isObject())
            return {};

        const GtsJsonValue* relationships = json.find("relationships");
        return relationships == nullptr ? UiSerializedSemanticRelationships{} : parseRelationshipIds(*relationships);
    }

    void readLocalizationKeyField(const GtsJsonValue& json,
                                  const std::string& field,
                                  std::string& outKey,
                                  bool& outPresent)
    {
        if (json.find(field) == nullptr)
            return;
        outKey = json.findString(field).value_or(outKey);
        outPresent = true;
    }

    UiSerializedSemanticLocalizationRefs parseSemanticLocalizationRefs(const GtsJsonValue& json)
    {
        UiSerializedSemanticLocalizationRefs refs;
        if (!json.isObject())
            return refs;

        readLocalizationKeyField(json, "nameKey", refs.nameKey, refs.hasNameKey);
        readLocalizationKeyField(json, "descriptionKey", refs.descriptionKey, refs.hasDescriptionKey);
        readLocalizationKeyField(json, "hintKey", refs.hintKey, refs.hasHintKey);
        readLocalizationKeyField(json, "valueKey", refs.valueKey, refs.hasValueKey);
        readLocalizationKeyField(json, "liveRegionTextKey", refs.valueKey, refs.hasValueKey);
        return refs;
    }

    void parseTopLevelSemanticLocalizationAliases(const GtsJsonValue& json,
                                                  UiSerializedSemanticLocalizationRefs& refs)
    {
        readLocalizationKeyField(json, "semanticNameKey", refs.nameKey, refs.hasNameKey);
        readLocalizationKeyField(json, "semanticDescriptionKey", refs.descriptionKey, refs.hasDescriptionKey);
        readLocalizationKeyField(json, "semanticHintKey", refs.hintKey, refs.hasHintKey);
        readLocalizationKeyField(json, "semanticValueKey", refs.valueKey, refs.hasValueKey);
        readLocalizationKeyField(json, "semanticLiveRegionTextKey", refs.valueKey, refs.hasValueKey);
    }

    GtsJsonValue serializeHandleArray(const std::vector<UiHandle>& values,
                                     const std::vector<std::string>& ids = {})
    {
        Array array;
        for (const std::string& id : ids)
            array.push_back(id);
        for (UiHandle handle : values)
        {
            if (handle != UI_INVALID_HANDLE)
                array.push_back(handle);
        }
        return array;
    }

    GtsJsonValue serializeRelationship(const UiSemanticRelationship& relationship,
                                      const UiSerializedSemanticRelationships* ids = nullptr)
    {
        return GtsJsonValue::Object({
            {"labelledBy", serializeHandleArray(relationship.labelledBy, ids == nullptr ? std::vector<std::string>{} : ids->labelledBy)},
            {"describedBy", serializeHandleArray(relationship.describedBy, ids == nullptr ? std::vector<std::string>{} : ids->describedBy)},
            {"controls", serializeHandleArray(relationship.controls, ids == nullptr ? std::vector<std::string>{} : ids->controls)},
            {"owns", serializeHandleArray(relationship.owns, ids == nullptr ? std::vector<std::string>{} : ids->owns)},
            {"activeDescendant", ids != nullptr && !ids->activeDescendant.empty()
                ? GtsJsonValue(ids->activeDescendant)
                : GtsJsonValue(relationship.activeDescendant)},
            {"popup", ids != nullptr && !ids->popup.empty() ? GtsJsonValue(ids->popup) : GtsJsonValue(relationship.popup)},
            {"tooltip", ids != nullptr && !ids->tooltip.empty() ? GtsJsonValue(ids->tooltip) : GtsJsonValue(relationship.tooltip)}
        });
    }

    UiSemanticDesc parseSemantic(const GtsJsonValue& json)
    {
        UiSemanticDesc semantic;
        if (!json.isObject())
            return semantic;

        if (const auto value = json.findString("role"))
            semantic.role = gts::enumValue(uiSemanticRoleNames, *value).value_or(semantic.role);
        semantic.name = json.findString("name").value_or(semantic.name);
        semantic.description = json.findString("description").value_or(semantic.description);
        semantic.hint = json.findString("hint").value_or(semantic.hint);
        semantic.value = json.findString("value").value_or(semantic.value);
        if (const auto value = json.findString("liveRegion"))
            semantic.liveRegion = gts::enumValue(uiAccessibilityLiveRegionNames, *value).value_or(semantic.liveRegion);
        semantic.hidden = json.findBool("hidden").value_or(semantic.hidden);
        semantic.decorative = json.findBool("decorative").value_or(semantic.decorative);
        semantic.selected = json.findBool("selected").value_or(semantic.selected);
        semantic.checked = json.findBool("checked").value_or(semantic.checked);
        semantic.expanded = json.findBool("expanded").value_or(semantic.expanded);
        semantic.readOnly = json.findBool("readOnly").value_or(semantic.readOnly);
        semantic.busy = json.findBool("busy").value_or(semantic.busy);
        semantic.hasRange = json.findBool("hasRange").value_or(semantic.hasRange);
        semantic.rangeMin = json.findFloat("rangeMin").value_or(semantic.rangeMin);
        semantic.rangeMax = json.findFloat("rangeMax").value_or(semantic.rangeMax);
        semantic.rangeValue = json.findFloat("rangeValue").value_or(semantic.rangeValue);
        semantic.level = json.findInt32("level").value_or(semantic.level);
        semantic.index = json.findInt32("index").value_or(semantic.index);
        semantic.count = json.findInt32("count").value_or(semantic.count);
        if (const GtsJsonValue* value = json.find("relationships"))
            semantic.relationships = parseRelationship(*value);
        return semantic;
    }

    GtsJsonValue serializeSemantic(const UiSemanticDesc& semantic,
                                  const UiSerializedSemanticRelationships* relationships = nullptr,
                                  const UiSerializedSemanticLocalizationRefs* localization = nullptr)
    {
        Object object = {
            {"role", std::string(gts::enumName(uiSemanticRoleNames, semantic.role).value_or("Unknown"))},
            {"name", semantic.name},
            {"description", semantic.description},
            {"hint", semantic.hint},
            {"value", semantic.value},
            {"liveRegion",
                 std::string(gts::enumName(uiAccessibilityLiveRegionNames, semantic.liveRegion).value_or("Off"))},
            {"relationships", serializeRelationship(semantic.relationships, relationships)},
            {"hidden", semantic.hidden},
            {"decorative", semantic.decorative},
            {"selected", semantic.selected},
            {"checked", semantic.checked},
            {"expanded", semantic.expanded},
            {"readOnly", semantic.readOnly},
            {"busy", semantic.busy},
            {"hasRange", semantic.hasRange},
            {"rangeMin", semantic.rangeMin},
            {"rangeMax", semantic.rangeMax},
            {"rangeValue", semantic.rangeValue},
            {"level", semantic.level},
            {"index", semantic.index},
            {"count", semantic.count}};
        if (localization != nullptr)
        {
            if (localization->hasNameKey || !localization->nameKey.empty())
                object.emplace_back("nameKey", localization->nameKey);
            if (localization->hasDescriptionKey || !localization->descriptionKey.empty())
                object.emplace_back("descriptionKey", localization->descriptionKey);
            if (localization->hasHintKey || !localization->hintKey.empty())
                object.emplace_back("hintKey", localization->hintKey);
            if (localization->hasValueKey || !localization->valueKey.empty())
                object.emplace_back("valueKey", localization->valueKey);
        }
        return object;
    }

    UiSerializedBinding parseBinding(const GtsJsonValue& json)
    {
        UiSerializedBinding binding;
        if (!json.isObject())
            return binding;
        if (const auto value = json.findString("property"))
            binding.property = gts::enumValue(uiBindablePropertyNames, *value).value_or(binding.property);
        binding.path = json.findString("path").value_or(binding.path);
        binding.formatter = json.findString("formatter").value_or(binding.formatter);
        binding.transform = json.findString("transform").value_or(binding.transform);
        binding.animateInitial = json.findBool("animateInitial").value_or(binding.animateInitial);
        binding.applyImmediately = json.findBool("applyImmediately").value_or(binding.applyImmediately);
        if (const GtsJsonValue* value = json.find("animation"))
            binding.animation = parseTiming(*value);
        return binding;
    }

    GtsJsonValue serializeBinding(const UiSerializedBinding& binding)
    {
        Object object = {
            {"property",
             std::string(gts::enumName(uiBindablePropertyNames, binding.property).value_or("Text"))},
            {"path", binding.path},
            {"formatter", binding.formatter},
            {"transform", binding.transform},
            {"animateInitial", binding.animateInitial},
            {"applyImmediately", binding.applyImmediately}};
        if (binding.animation)
            object.emplace_back("animation", serializeTiming(*binding.animation));
        return object;
    }

    UiSerializedNavigation parseNavigation(const GtsJsonValue& json)
    {
        UiSerializedNavigation navigation;
        if (!json.isObject())
            return navigation;
        navigation.enabled = true;
        navigation.focusable = json.findBool("focusable").value_or(navigation.focusable);
        navigation.enabled = json.findBool("enabled").value_or(navigation.enabled);
        if (const auto value = json.findString("role"))
            navigation.role = gts::enumValue(uiNavigationRoleNames, *value).value_or(navigation.role);
        navigation.scope = json.findUInt32("scope").value_or(navigation.scope);
        navigation.group = json.findString("group").value_or(navigation.group);
        navigation.tabIndex = json.findInt32("tabIndex").value_or(navigation.tabIndex);
        navigation.wrapNavigation = json.findBool("wrapNavigation").value_or(navigation.wrapNavigation);
        navigation.activateOnSubmit = json.findBool("activateOnSubmit").value_or(navigation.activateOnSubmit);
        if (const GtsJsonValue* neighbors = json.find("neighbors"))
        {
            if (const Object* object = neighbors->tryObject())
            {
                for (const auto& [key, value] : *object)
                {
                    if (const auto string = value.tryString())
                    {
                        const UiNavigationDirection direction =
                            gts::enumValue(uiNavigationDirectionNames, key).value_or(UiNavigationDirection::None);
                        if (direction != UiNavigationDirection::None)
                            navigation.neighbors[direction] = *string;
                    }
                }
            }
        }
        return navigation;
    }

    GtsJsonValue serializeNavigation(const UiSerializedNavigation& navigation)
    {
        Object neighbors;
        for (const auto& [direction, target] : navigation.neighbors)
            neighbors.emplace_back(std::string(gts::enumName(uiNavigationDirectionNames, direction).value_or("None")),
                                   target);

        return GtsJsonValue::Object(
            {{"enabled", navigation.enabled},
             {"focusable", navigation.focusable},
             {"role",
              std::string(gts::enumName(uiNavigationRoleNames, navigation.role).value_or("Generic"))},
             {"scope", navigation.scope},
             {"group", navigation.group},
             {"tabIndex", navigation.tabIndex},
             {"wrapNavigation", navigation.wrapNavigation},
             {"activateOnSubmit", navigation.activateOnSubmit},
             {"neighbors", std::move(neighbors)}});
    }

    UiSerializedDragSource parseDragSource(const GtsJsonValue& json)
    {
        UiSerializedDragSource source;
        if (!json.isObject())
            return source;
        source.enabled = json.findBool("enabled").value_or(source.enabled);
        source.payloadType = json.findString("payloadType").value_or(source.payloadType);
        source.payloadId = json.findUInt64("payloadId").value_or(source.payloadId);
        source.payloadLabel = json.findString("payloadLabel").value_or(source.payloadLabel);
        source.startThreshold = json.findFloat("startThreshold").value_or(source.startThreshold);
        source.capturePointer = json.findBool("capturePointer").value_or(source.capturePointer);
        return source;
    }

    GtsJsonValue serializeDragSource(const UiSerializedDragSource& source)
    {
        return GtsJsonValue::Object({
            {"enabled", source.enabled},
            {"payloadType", source.payloadType},
            {"payloadId", source.payloadId},
            {"payloadLabel", source.payloadLabel},
            {"startThreshold", source.startThreshold},
            {"capturePointer", source.capturePointer}
        });
    }

    UiSerializedDropTarget parseDropTarget(const GtsJsonValue& json)
    {
        UiSerializedDropTarget target;
        if (!json.isObject())
            return target;
        target.enabled = json.findBool("enabled").value_or(target.enabled);
        target.acceptsAnyPayload = json.findBool("acceptsAnyPayload").value_or(target.acceptsAnyPayload);
        if (const GtsJsonValue* accepted = json.find("acceptedPayloadTypes"))
        {
            if (const Array* array = accepted->tryArray())
            {
                for (const GtsJsonValue& value : *array)
                {
                    if (const auto string = value.tryString())
                        target.acceptedPayloadTypes.push_back(*string);
                }
            }
        }
        return target;
    }

    GtsJsonValue serializeDropTarget(const UiSerializedDropTarget& target)
    {
        Array accepted;
        for (const std::string& type : target.acceptedPayloadTypes)
            accepted.push_back(type);
        return GtsJsonValue::Object({
            {"enabled", target.enabled},
            {"acceptsAnyPayload", target.acceptsAnyPayload},
            {"acceptedPayloadTypes", std::move(accepted)}
        });
    }

    std::optional<UiStyleTransitionDesc> parseStyleTransition(const GtsJsonValue& json)
    {
        if (!json.isObject())
            return std::nullopt;
        UiStyleTransitionDesc transition;
        if (const GtsJsonValue* value = json.find("timing"))
            transition.timing = parseTiming(*value);
        transition.animateBackground = json.findBool("animateBackground").value_or(transition.animateBackground);
        transition.animateForeground = json.findBool("animateForeground").value_or(transition.animateForeground);
        transition.animateOpacity = json.findBool("animateOpacity").value_or(transition.animateOpacity);
        return transition;
    }

    GtsJsonValue serializeStyleTransition(const UiStyleTransitionDesc& transition)
    {
        return GtsJsonValue::Object({
            {"timing", serializeTiming(transition.timing)},
            {"animateBackground", transition.animateBackground},
            {"animateForeground", transition.animateForeground},
            {"animateOpacity", transition.animateOpacity}
        });
    }

    UiSerializedWidget parseWidget(const GtsJsonValue& json)
    {
        UiSerializedWidget widget;
        if (!json.isObject())
            return widget;
        widget.id = json.findString("id").value_or(widget.id);
        widget.type = json.findString("type").value_or(widget.type);
        widget.asset = json.findString("asset").value_or(widget.asset);
        widget.variant = json.findString("variant").value_or(widget.variant);
        widget.text = json.findString("text").value_or(widget.text);
        if (json.find("textKey") != nullptr)
        {
            widget.textKey = json.findString("textKey").value_or(widget.textKey);
            widget.hasTextKey = true;
        }
        widget.styleClass = json.findString("styleClass").value_or(widget.styleClass);
        widget.labelStyleClass = json.findString("labelStyleClass").value_or(widget.labelStyleClass);
        widget.imageAsset = json.findString("imageAsset").value_or(widget.imageAsset);
        if (const GtsJsonValue* parameters = json.find("parameters"))
        {
            if (const Object* object = parameters->tryObject())
            {
                for (const auto& [key, value] : *object)
                {
                    if (const auto string = value.tryString())
                        widget.parameters[key] = *string;
                    else if (value.isNumber() || value.isBool())
                        widget.parameters[key] = GtsJsonParser::serialize(value);
                }
            }
        }
        if (const auto value = json.findString("horizontalAlign"))
            widget.horizontalAlign = gts::enumValue(uiHorizontalAlignNames, *value).value_or(widget.horizontalAlign);
        if (const auto value = json.findString("verticalAlign"))
            widget.verticalAlign = gts::enumValue(uiVerticalAlignNames, *value).value_or(widget.verticalAlign);
        if (const auto value = json.findString("wrapMode"))
            widget.wrapMode = gts::enumValue(uiTextWrapModeNames, *value).value_or(widget.wrapMode);
        widget.maxLines = json.findInt32("maxLines").value_or(widget.maxLines);
        if (const GtsJsonValue* value = json.find("imageTint")) widget.imageTint = parseColor(*value, widget.imageTint);
        widget.imageAspect = json.findFloat("imageAspect").value_or(widget.imageAspect);
        widget.rotation = json.findFloat("rotation").value_or(widget.rotation);
        widget.progressValue = json.findFloat("value").value_or(widget.progressValue);
        if (const GtsJsonValue* value = json.find("contentOffset")) widget.contentOffset = parseVec2(*value, widget.contentOffset);
        if (json.find("visible") != nullptr)
        {
            widget.visible = json.findBool("visible").value_or(widget.visible);
            widget.hasVisible = true;
        }
        if (json.find("enabled") != nullptr)
        {
            widget.enabled = json.findBool("enabled").value_or(widget.enabled);
            widget.hasEnabled = true;
        }
        if (json.find("interactable") != nullptr)
        {
            widget.interactable = json.findBool("interactable").value_or(widget.interactable);
            widget.hasInteractable = true;
        }
        if (json.find("decorative") != nullptr)
        {
            widget.decorative = json.findBool("decorative").value_or(widget.decorative);
            widget.hasDecorative = true;
        }
        if (const GtsJsonValue* value = json.find("layout"))
        {
            widget.layout = parseLayout(*value, widget.layout);
            widget.hasLayout = true;
        }
        if (const GtsJsonValue* value = json.find("semantics"))
        {
            widget.semantics = parseSemantic(*value);
            widget.semanticLocalization = parseSemanticLocalizationRefs(*value);
            widget.semanticRelationships = parseSemanticRelationshipIds(*value);
            widget.hasSemanticRelationships = hasRelationshipIds(widget.semanticRelationships);
            widget.hasSemantics = true;
        }
        parseTopLevelSemanticLocalizationAliases(json, widget.semanticLocalization);
        if (widget.semanticLocalization.any())
            widget.hasSemantics = true;
        if (const GtsJsonValue* value = json.find("navigation"))
        {
            widget.navigation = parseNavigation(*value);
            widget.hasNavigation = true;
        }
        if (const GtsJsonValue* value = json.find("dragSource")) widget.dragSource = parseDragSource(*value);
        if (const GtsJsonValue* value = json.find("dropTarget")) widget.dropTarget = parseDropTarget(*value);
        if (const GtsJsonValue* value = json.find("stateTransition")) widget.stateTransition = parseStyleTransition(*value);
        if (const GtsJsonValue* value = json.find("bindings"))
        {
            if (const Array* array = value->tryArray())
            {
                for (const GtsJsonValue& item : *array)
                    widget.bindings.push_back(parseBinding(item));
            }
        }
        if (const GtsJsonValue* value = json.find("children"))
        {
            if (const Array* array = value->tryArray())
            {
                for (const GtsJsonValue& item : *array)
                    widget.children.push_back(parseWidget(item));
            }
        }
        if (const GtsJsonValue* value = json.find("slots"))
        {
            if (const Object* object = value->tryObject())
            {
                for (const auto& [slotName, slotValue] : *object)
                {
                    if (const Array* array = slotValue.tryArray())
                    {
                        std::vector<UiSerializedWidget>& children = widget.slots[slotName];
                        for (const GtsJsonValue& item : *array)
                            children.push_back(parseWidget(item));
                    }
                }
            }
        }
        return widget;
    }

    GtsJsonValue serializeWidget(const UiSerializedWidget& widget)
    {
        Array bindings;
        for (const UiSerializedBinding& binding : widget.bindings)
            bindings.push_back(serializeBinding(binding));
        Array children;
        for (const UiSerializedWidget& child : widget.children)
            children.push_back(serializeWidget(child));
        Object parameters;
        for (const auto& [key, value] : widget.parameters)
            parameters.emplace_back(key, value);
        Object slots;
        for (const auto& [slotName, slotChildren] : widget.slots)
        {
            Array slotArray;
            for (const UiSerializedWidget& child : slotChildren)
                slotArray.push_back(serializeWidget(child));
            slots.emplace_back(slotName, std::move(slotArray));
        }

        Object object = {
            {"id", widget.id},
            {"type", widget.type},
            {"asset", widget.asset},
            {"variant", widget.variant},
            {"parameters", std::move(parameters)},
            {"layout", serializeLayout(widget.layout)},
            {"styleClass", widget.styleClass},
            {"labelStyleClass", widget.labelStyleClass},
            {"text", widget.text},
            {"imageAsset", widget.imageAsset},
            {"horizontalAlign",
             std::string(gts::enumName(uiHorizontalAlignNames, widget.horizontalAlign).value_or("Left"))},
            {"verticalAlign",
             std::string(gts::enumName(uiVerticalAlignNames, widget.verticalAlign).value_or("Top"))},
            {"wrapMode",
             std::string(gts::enumName(uiTextWrapModeNames, widget.wrapMode).value_or("None"))},
            {"maxLines", widget.maxLines},
            {"imageTint", Array{widget.imageTint.r, widget.imageTint.g, widget.imageTint.b, widget.imageTint.a}},
            {"imageAspect", widget.imageAspect},
            {"rotation", widget.rotation},
            {"value", widget.progressValue},
            {"contentOffset", Array{widget.contentOffset.x, widget.contentOffset.y}},
            {"visible", widget.visible},
            {"enabled", widget.enabled},
            {"interactable", widget.interactable},
            {"decorative", widget.decorative},
            {"navigation", serializeNavigation(widget.navigation)},
            {"bindings", std::move(bindings)},
            {"children", std::move(children)},
            {"slots", std::move(slots)}};
        if (widget.hasTextKey || !widget.textKey.empty())
            object.emplace_back("textKey", widget.textKey);
        if (widget.hasSemantics)
        {
            object.emplace_back("semantics",
                                serializeSemantic(widget.semantics,
                                                  widget.hasSemanticRelationships ? &widget.semanticRelationships
                                                                                  : nullptr,
                                                  widget.semanticLocalization.any() ? &widget.semanticLocalization
                                                                                   : nullptr));
        }
        if (widget.dragSource)
            object.emplace_back("dragSource", serializeDragSource(*widget.dragSource));
        if (widget.dropTarget)
            object.emplace_back("dropTarget", serializeDropTarget(*widget.dropTarget));
        if (widget.stateTransition)
            object.emplace_back("stateTransition", serializeStyleTransition(*widget.stateTransition));
        return object;
    }

    UiSerializedSurface parseSurface(const GtsJsonValue& json)
    {
        UiSerializedSurface surface;
        if (!json.isObject())
            return surface;
        surface.name = json.findString("name").value_or(surface.name);
        if (const auto value = json.findString("kind"))
            surface.kind = gts::enumValue(uiSurfaceKindNames, *value).value_or(surface.kind);
        surface.order = json.findInt32("order").value_or(surface.order);
        if (const GtsJsonValue* value = json.find("rect"))
            surface.rect = parseRect(*value, surface.rect);
        surface.visible = json.findBool("visible").value_or(surface.visible);
        surface.enabled = json.findBool("enabled").value_or(surface.enabled);
        surface.inputEnabled = json.findBool("inputEnabled").value_or(surface.inputEnabled);
        surface.renderEnabled = json.findBool("renderEnabled").value_or(surface.renderEnabled);
        return surface;
    }

    GtsJsonValue serializeSurface(const UiSerializedSurface& surface)
    {
        return GtsJsonValue::Object({
            {"name", surface.name},
            {"kind", std::string(gts::enumName(uiSurfaceKindNames, surface.kind).value_or("Screen"))},
            {"order", surface.order},
            {"rect", Array{surface.rect.x, surface.rect.y, surface.rect.width, surface.rect.height}},
            {"visible", surface.visible},
            {"enabled", surface.enabled},
            {"inputEnabled", surface.inputEnabled},
            {"renderEnabled", surface.renderEnabled}
        });
    }

    UiSerializedLayer parseLayer(const GtsJsonValue& json)
    {
        UiSerializedLayer layer;
        if (!json.isObject())
            return layer;
        layer.name = json.findString("name").value_or(layer.name);
        layer.order = json.findInt32("order").value_or(layer.order);
        layer.state.visible = json.findBool("visible").value_or(layer.state.visible);
        layer.state.inputEnabled = json.findBool("inputEnabled").value_or(layer.state.inputEnabled);
        return layer;
    }

    GtsJsonValue serializeLayer(const UiSerializedLayer& layer)
    {
        return GtsJsonValue::Object({
            {"name", layer.name},
            {"order", layer.order},
            {"visible", layer.state.visible},
            {"inputEnabled", layer.state.inputEnabled}
        });
    }

    GtsJsonValue serializeAssetJson(const UiSerializedAsset& asset)
    {
        Object object = {
            {"schema", asset.schemaVersion},
            {"id", asset.id},
            {"theme", asset.theme},
            {"root", serializeWidget(asset.root)}
        };
        if (asset.surface)
            object.emplace_back("surface", serializeSurface(*asset.surface));

        Array layers;
        for (const UiSerializedLayer& layer : asset.layers)
            layers.push_back(serializeLayer(layer));
        object.emplace_back("layers", std::move(layers));
        return object;
    }

    bool isKnownWidgetType(const std::string& type)
    {
        static const std::vector<std::string> known = {
            "Label", "Panel", "Button", "Image", "ProgressBar",
            "Spacer", "Stack", "Separator", "ScrollView"
        };
        return std::find(known.begin(), known.end(), type) != known.end();
    }

    bool hasTemplateToken(const std::string& value)
    {
        return value.find("{{") != std::string::npos || value.find("}}") != std::string::npos;
    }

    bool isValidLocalizationKeyCharacter(char ch)
    {
        const unsigned char value = static_cast<unsigned char>(ch);
        return std::isalnum(value) != 0 || ch == '_' || ch == '-' || ch == '.';
    }

    bool isValidLocalizationKeyRef(const std::string& key)
    {
        if (key.empty())
            return false;
        if (key.front() == '.' || key.back() == '.')
            return false;
        if (key.find("..") != std::string::npos)
            return false;
        return std::all_of(key.begin(), key.end(), isValidLocalizationKeyCharacter);
    }

    void validateLocalizationKeyRef(const std::string& key,
                                    bool present,
                                    const std::string& path,
                                    UiSerializedValidationResult& result)
    {
        if (!present && key.empty())
            return;
        if (key.empty())
        {
            result.error(path, "localization key reference must not be empty");
            return;
        }
        if (hasTemplateToken(key))
            return;
        if (!isValidLocalizationKeyRef(key))
            result.error(path, "malformed localization key reference '" + key + "'");
    }

    void validateWidget(const UiSerializedWidget& widget,
                        const UiTheme* theme,
                        const UiWidgetAssetRegistry* widgetAssets,
                        UiSerializedValidationResult& result,
                        const std::string& path,
                        std::unordered_map<std::string, int>& ids,
                        std::vector<std::pair<std::string, std::string>>& relationshipRefs)
    {
        if (widget.type.empty() && widget.asset.empty())
            result.error(path, "widget type or widget asset is required");
        else if (!widget.type.empty() && !isKnownWidgetType(widget.type))
            result.error(path, "unknown widget type '" + widget.type + "'");
        if (!widget.asset.empty() && widgetAssets != nullptr && widgetAssets->findAsset(widget.asset) == nullptr)
            result.error(path, "unknown widget asset '" + widget.asset + "'");

        validateLocalizationKeyRef(widget.textKey, widget.hasTextKey, path + ".textKey", result);
        if (widget.hasSemantics || widget.semanticLocalization.any())
        {
            validateLocalizationKeyRef(widget.semanticLocalization.nameKey,
                                      widget.semanticLocalization.hasNameKey,
                                      path + ".semantics.nameKey",
                                      result);
            validateLocalizationKeyRef(widget.semanticLocalization.descriptionKey,
                                      widget.semanticLocalization.hasDescriptionKey,
                                      path + ".semantics.descriptionKey",
                                      result);
            validateLocalizationKeyRef(widget.semanticLocalization.hintKey,
                                      widget.semanticLocalization.hasHintKey,
                                      path + ".semantics.hintKey",
                                      result);
            validateLocalizationKeyRef(widget.semanticLocalization.valueKey,
                                      widget.semanticLocalization.hasValueKey,
                                      path + ".semantics.valueKey",
                                      result);
        }

        if (!widget.id.empty())
            ++ids[widget.id];
        if (widget.hasSemanticRelationships)
        {
            const auto collectRefs = [&](const std::vector<std::string>& refs, const std::string& refPath)
            {
                for (const std::string& ref : refs)
                    relationshipRefs.emplace_back(refPath, ref);
            };
            collectRefs(widget.semanticRelationships.labelledBy, path + ".semantics.relationships.labelledBy");
            collectRefs(widget.semanticRelationships.describedBy, path + ".semantics.relationships.describedBy");
            collectRefs(widget.semanticRelationships.controls, path + ".semantics.relationships.controls");
            collectRefs(widget.semanticRelationships.owns, path + ".semantics.relationships.owns");
            if (!widget.semanticRelationships.activeDescendant.empty())
                relationshipRefs.emplace_back(path + ".semantics.relationships.activeDescendant",
                                              widget.semanticRelationships.activeDescendant);
            if (!widget.semanticRelationships.popup.empty())
                relationshipRefs.emplace_back(path + ".semantics.relationships.popup",
                                              widget.semanticRelationships.popup);
            if (!widget.semanticRelationships.tooltip.empty())
                relationshipRefs.emplace_back(path + ".semantics.relationships.tooltip",
                                              widget.semanticRelationships.tooltip);
        }

        if (theme != nullptr && !widget.styleClass.empty() && theme->findStyleClass(widget.styleClass) == nullptr)
            result.error(path, "missing style class '" + widget.styleClass + "'");
        if (theme != nullptr && !widget.labelStyleClass.empty() && theme->findStyleClass(widget.labelStyleClass) == nullptr)
            result.error(path, "missing label style class '" + widget.labelStyleClass + "'");

        if (widget.layout.gridColumns <= 0)
            result.error(path + ".layout", "gridColumns must be positive");
        if (widget.layout.gridRows <= 0)
            result.error(path + ".layout", "gridRows must be positive");
        if (widget.layout.gridColumnSpan <= 0 || widget.layout.gridRowSpan <= 0)
            result.error(path + ".layout", "grid spans must be positive");

        for (size_t i = 0; i < widget.bindings.size(); ++i)
        {
            if (widget.bindings[i].path.empty())
                result.error(path + ".bindings[" + std::to_string(i) + "]", "binding path is required");
        }

        for (size_t i = 0; i < widget.children.size(); ++i)
            validateWidget(widget.children[i],
                           theme,
                           widgetAssets,
                           result,
                           path + ".children[" + std::to_string(i) + "]",
                           ids,
                           relationshipRefs);
        for (const auto& [slotName, slotChildren] : widget.slots)
        {
            for (size_t i = 0; i < slotChildren.size(); ++i)
            {
                validateWidget(slotChildren[i],
                               theme,
                               widgetAssets,
                               result,
                               path + ".slots." + slotName + "[" + std::to_string(i) + "]",
                               ids,
                               relationshipRefs);
            }
        }
    }

    bool resolveRelationshipRef(const std::unordered_map<std::string, UiHandle>& handles,
                                const std::string& id,
                                UiHandle& outHandle)
    {
        if (id.empty())
            return true;
        const auto it = handles.find(id);
        if (it == handles.end())
            return false;
        outHandle = it->second;
        return true;
    }

    bool appendRelationshipRefs(const std::unordered_map<std::string, UiHandle>& handles,
                                const std::vector<std::string>& ids,
                                std::vector<UiHandle>& outHandles)
    {
        bool ok = true;
        for (const std::string& id : ids)
        {
            UiHandle handle = UI_INVALID_HANDLE;
            if (!resolveRelationshipRef(handles, id, handle))
            {
                ok = false;
                continue;
            }
            outHandles.push_back(handle);
        }
        return ok;
    }

    bool resolveSemanticRelationshipIds(const std::unordered_map<std::string, UiHandle>& handles,
                                        const UiSerializedSemanticRelationships& ids,
                                        UiSemanticRelationship& outRelationship)
    {
        bool ok = true;
        ok = appendRelationshipRefs(handles, ids.labelledBy, outRelationship.labelledBy) && ok;
        ok = appendRelationshipRefs(handles, ids.describedBy, outRelationship.describedBy) && ok;
        ok = appendRelationshipRefs(handles, ids.controls, outRelationship.controls) && ok;
        ok = appendRelationshipRefs(handles, ids.owns, outRelationship.owns) && ok;
        ok = resolveRelationshipRef(handles, ids.activeDescendant, outRelationship.activeDescendant) && ok;
        ok = resolveRelationshipRef(handles, ids.popup, outRelationship.popup) && ok;
        ok = resolveRelationshipRef(handles, ids.tooltip, outRelationship.tooltip) && ok;
        return ok;
    }

    UiDragSourceDesc makeDragSourceDesc(const UiSerializedDragSource& source)
    {
        UiDragSourceDesc desc;
        desc.enabled = source.enabled;
        desc.payload.type = source.payloadType;
        desc.payload.id = source.payloadId;
        desc.payload.label = source.payloadLabel;
        desc.startThreshold = source.startThreshold;
        desc.capturePointer = source.capturePointer;
        return desc;
    }

    UiDropTargetDesc makeDropTargetDesc(const UiSerializedDropTarget& target)
    {
        UiDropTargetDesc desc;
        desc.enabled = target.enabled;
        desc.acceptsAnyPayload = target.acceptsAnyPayload;
        desc.acceptedPayloadTypes = target.acceptedPayloadTypes;
        return desc;
    }

    std::string localizationNamespaceFromAssetId(const std::string& assetId)
    {
        const size_t separator = assetId.rfind('.');
        if (separator == std::string::npos)
            return {};
        return assetId.substr(0, separator);
    }

    UiLocalizationScope localizationScopeForAsset(const UiSerializedAsset& asset)
    {
        UiLocalizationScope scope;
        scope.namespaceId = localizationNamespaceFromAssetId(asset.id);
        return scope;
    }

    struct InstantiationContext
    {
        UiSystem& ui;
        UiSurfaceId surface = UI_DEFAULT_SURFACE;
        UiMountId mount = UI_INVALID_MOUNT;
        const IUiSerializedBindingResolver* bindingResolver = nullptr;
        UiSerializedLoadResult& result;
        UiLocalizationScope localizationScope;
    };

    std::string resolveLocalizedString(InstantiationContext& context,
                                       const std::string& key,
                                       const std::string& fallbackText)
    {
        if (key.empty())
            return fallbackText;

        UiLocalizedTextRef ref;
        ref.key.key = key;
        ref.fallbackText = fallbackText;
        return context.ui.localize(ref, context.localizationScope).text;
    }

    UiSemanticDesc resolveLocalizedSemantics(InstantiationContext& context,
                                             const UiSerializedWidget& widget)
    {
        UiSemanticDesc desc = widget.semantics;
        const UiSerializedSemanticLocalizationRefs& refs = widget.semanticLocalization;
        if (refs.hasNameKey || !refs.nameKey.empty())
            desc.name = resolveLocalizedString(context, refs.nameKey, desc.name);
        if (refs.hasDescriptionKey || !refs.descriptionKey.empty())
            desc.description = resolveLocalizedString(context, refs.descriptionKey, desc.description);
        if (refs.hasHintKey || !refs.hintKey.empty())
            desc.hint = resolveLocalizedString(context, refs.hintKey, desc.hint);
        if (refs.hasValueKey || !refs.valueKey.empty())
            desc.value = resolveLocalizedString(context, refs.valueKey, desc.value);
        return desc;
    }

    UiBindingId bindSerialized(InstantiationContext& context,
                               UiHandle target,
                               UiHandle accessibilityTarget,
                               const UiSerializedBinding& binding)
    {
        if (context.bindingResolver == nullptr)
        {
            context.result.validation.error("bindings", "binding resolver is required for path '" + binding.path + "'");
            return UI_INVALID_BINDING;
        }

        std::optional<UiBindingSource> source =
            context.bindingResolver->resolveBindingSource(binding.path);
        if (!source)
        {
            context.result.validation.error("bindings", "unresolved binding path '" + binding.path + "'");
            return UI_INVALID_BINDING;
        }

        UiBindingDesc desc;
        desc.target = target;
        desc.accessibilityTarget = accessibilityTarget;
        desc.property = binding.property;
        desc.source = std::move(*source);
        desc.formatter = context.bindingResolver->resolveFormatter(binding.formatter);
        desc.transform = context.bindingResolver->resolveTransform(binding.transform);
        desc.ownerMount = context.mount;
        desc.animation = binding.animation;
        desc.animateInitial = binding.animateInitial;
        desc.applyImmediately = binding.applyImmediately;
        desc.debugName = binding.path;
        return context.ui.bind(context.surface, desc);
    }

    void applySerializedBindings(InstantiationContext& context,
                                 UiHandle root,
                                 UiHandle progressFill,
                                 const UiSerializedWidget& widget)
    {
        for (const UiSerializedBinding& binding : widget.bindings)
        {
            UiHandle target = root;
            UiHandle accessibilityTarget = root;
            if (widget.type == "ProgressBar" && binding.property == UiBindableProperty::Progress)
                target = progressFill;
            bindSerialized(context, target, accessibilityTarget, binding);
        }
    }

    void applyNavigation(InstantiationContext& context,
                         UiHandle root,
                         const UiSerializedWidget& widget)
    {
        if (!widget.navigation.enabled || root == UI_INVALID_HANDLE)
            return;

        UiNavigationNodeDesc desc;
        desc.focusable = widget.navigation.focusable;
        desc.enabled = widget.navigation.enabled;
        desc.role = widget.navigation.role;
        desc.scope = widget.navigation.scope;
        desc.group = widget.navigation.group;
        desc.tabIndex = widget.navigation.tabIndex;
        desc.wrapNavigation = widget.navigation.wrapNavigation;
        desc.activateOnSubmit = widget.navigation.activateOnSubmit;
        context.ui.registerNavigationNode(context.surface, root, desc);
    }

    void applyDragDrop(InstantiationContext& context,
                       UiHandle root,
                       const UiSerializedWidget& widget)
    {
        if (root == UI_INVALID_HANDLE)
            return;
        if (widget.dragSource)
            context.ui.registerDragSource(context.surface, root, makeDragSourceDesc(*widget.dragSource));
        if (widget.dropTarget)
            context.ui.registerDropTarget(context.surface, root, makeDropTargetDesc(*widget.dropTarget));
    }

    UiHandle instantiateWidget(InstantiationContext& context,
                               UiHandle parent,
                               const UiSerializedWidget& widget)
    {
        UiDocument* document = context.ui.findDocument(context.surface);
        if (document == nullptr)
        {
            context.result.validation.error(widget.id, "surface document missing");
            return UI_INVALID_HANDLE;
        }
        UiCompositionContext compositionContext{context.ui, *document, nullptr, context.surface, context.mount, parent};
        gts::ui::UiWidgetContext widgetContext(compositionContext);

        UiHandle root = UI_INVALID_HANDLE;
        UiHandle childParent = UI_INVALID_HANDLE;
        UiHandle progressFill = UI_INVALID_HANDLE;
        const std::string resolvedText =
            resolveLocalizedString(context, widget.textKey, widget.text);
        const UiSemanticDesc resolvedSemantics =
            widget.hasSemantics ? resolveLocalizedSemantics(context, widget) : UiSemanticDesc{};

        if (widget.type == "Label")
        {
            gts::ui::UiLabelWidget label;
            gts::ui::UiLabelDesc desc;
            desc.layout = widget.layout;
            desc.text = resolvedText;
            desc.styleClass = widget.styleClass.empty() ? desc.styleClass : widget.styleClass;
            desc.visible = widget.visible;
            desc.horizontalAlign = widget.horizontalAlign;
            desc.verticalAlign = widget.verticalAlign;
            desc.wrapMode = widget.wrapMode;
            desc.maxLines = widget.maxLines;
            if (widget.hasSemantics)
            {
                desc.semanticRole = resolvedSemantics.role;
                desc.accessibilityName = resolvedSemantics.name;
                desc.accessibilityDescription = resolvedSemantics.description;
                desc.accessibilityHint = resolvedSemantics.hint;
                desc.liveRegion = resolvedSemantics.liveRegion;
                desc.accessibilityHidden = resolvedSemantics.hidden;
            }
            label.build(widgetContext, parent, desc);
            root = label.root();
            childParent = root;
        }
        else if (widget.type == "Panel")
        {
            gts::ui::UiPanelWidget panel;
            gts::ui::UiPanelDesc desc;
            desc.layout = widget.layout;
            desc.styleClass = widget.styleClass.empty() ? desc.styleClass : widget.styleClass;
            desc.visible = widget.visible;
            desc.enabled = widget.enabled;
            desc.interactable = widget.interactable;
            if (widget.dragSource) desc.dragSource = makeDragSourceDesc(*widget.dragSource);
            if (widget.dropTarget) desc.dropTarget = makeDropTargetDesc(*widget.dropTarget);
            panel.build(widgetContext, parent, desc);
            root = panel.root();
            childParent = panel.content();
            if (!widget.id.empty())
                context.result.instance.handles[widget.id + ".content"] = childParent;
        }
        else if (widget.type == "Button")
        {
            gts::ui::UiButtonWidget button;
            gts::ui::UiButtonDesc desc;
            desc.layout = widget.layout;
            desc.text = resolvedText;
            desc.styleClass = widget.styleClass.empty() ? desc.styleClass : widget.styleClass;
            desc.labelStyleClass = widget.labelStyleClass.empty() ? desc.labelStyleClass : widget.labelStyleClass;
            desc.visible = widget.visible;
            desc.enabled = widget.enabled;
            desc.horizontalAlign = widget.horizontalAlign;
            desc.verticalAlign = widget.verticalAlign;
            desc.wrapMode = widget.wrapMode;
            desc.maxLines = widget.maxLines;
            desc.focusable = widget.navigation.enabled && widget.navigation.focusable;
            desc.navigationRole = widget.navigation.role;
            desc.navigationGroup = widget.navigation.group;
            desc.tabIndex = widget.navigation.tabIndex;
            desc.wrapNavigation = widget.navigation.wrapNavigation;
            if (widget.hasSemantics)
            {
                desc.semanticRole = resolvedSemantics.role;
                desc.accessibilityName = resolvedSemantics.name;
                desc.accessibilityDescription = resolvedSemantics.description;
                desc.accessibilityHint = resolvedSemantics.hint;
                desc.liveRegion = resolvedSemantics.liveRegion;
            }
            if (widget.dragSource) desc.dragSource = makeDragSourceDesc(*widget.dragSource);
            if (widget.dropTarget) desc.dropTarget = makeDropTargetDesc(*widget.dropTarget);
            if (widget.stateTransition) desc.stateAnimation = *widget.stateTransition;
            button.build(widgetContext, parent, desc);
            root = button.root();
            childParent = root;
            if (!widget.id.empty())
                context.result.instance.handles[widget.id + ".label"] = button.label();
        }
        else if (widget.type == "Image")
        {
            gts::ui::UiImageWidget image;
            gts::ui::UiImageDesc desc;
            desc.layout = widget.layout;
            desc.imageAsset = widget.imageAsset;
            desc.tint = widget.imageTint;
            desc.imageAspect = widget.imageAspect;
            desc.rotation = widget.rotation;
            desc.visible = widget.visible;
            desc.decorative = widget.decorative;
            if (widget.hasSemantics)
            {
                desc.accessibilityName = resolvedSemantics.name;
                desc.accessibilityDescription = resolvedSemantics.description;
            }
            image.build(widgetContext, parent, desc);
            root = image.root();
            childParent = root;
        }
        else if (widget.type == "ProgressBar")
        {
            gts::ui::UiProgressBarWidget progress;
            gts::ui::UiProgressBarDesc desc;
            desc.layout = widget.layout;
            desc.trackStyleClass = widget.styleClass.empty() ? desc.trackStyleClass : widget.styleClass;
            desc.value = widget.progressValue;
            desc.visible = widget.visible;
            if (widget.hasSemantics)
            {
                desc.accessibilityName = resolvedSemantics.name;
                desc.accessibilityDescription = resolvedSemantics.description;
                desc.accessibilityHint = resolvedSemantics.hint;
                desc.liveRegion = resolvedSemantics.liveRegion;
            }
            progress.build(widgetContext, parent, desc);
            root = progress.root();
            childParent = root;
            progressFill = progress.fill();
            if (!widget.id.empty())
                context.result.instance.handles[widget.id + ".fill"] = progressFill;
        }
        else if (widget.type == "Spacer")
        {
            gts::ui::UiSpacerWidget spacer;
            gts::ui::UiSpacerDesc desc;
            desc.layout = widget.layout;
            desc.visible = widget.visible;
            spacer.build(widgetContext, parent, desc);
            root = spacer.root();
            childParent = root;
        }
        else if (widget.type == "Stack")
        {
            gts::ui::UiStackWidget stack;
            gts::ui::UiStackDesc desc;
            desc.layout = widget.layout;
            desc.axis = widget.layout.stackAxis;
            desc.mainAxisAlignment = widget.layout.mainAxisAlignment;
            desc.crossAxisAlignment = widget.layout.crossAxisAlignment;
            desc.gap = widget.layout.gap;
            desc.visible = widget.visible;
            stack.build(widgetContext, parent, desc);
            root = stack.root();
            childParent = root;
        }
        else if (widget.type == "Separator")
        {
            gts::ui::UiSeparatorWidget separator;
            gts::ui::UiSeparatorDesc desc;
            desc.layout = widget.layout;
            desc.styleClass = widget.styleClass.empty() ? desc.styleClass : widget.styleClass;
            desc.visible = widget.visible;
            separator.build(widgetContext, parent, desc);
            root = separator.root();
            childParent = root;
        }
        else if (widget.type == "ScrollView")
        {
            gts::ui::UiScrollViewWidget scroll;
            gts::ui::UiScrollViewDesc desc;
            desc.layout = widget.layout;
            desc.contentOffset = widget.contentOffset;
            desc.visible = widget.visible;
            scroll.build(widgetContext, parent, desc);
            root = scroll.root();
            childParent = root;
        }

        if (root == UI_INVALID_HANDLE)
        {
            context.result.validation.error(widget.id, "failed to instantiate widget '" + widget.type + "'");
            return UI_INVALID_HANDLE;
        }

        if (!widget.id.empty())
            context.result.instance.handles[widget.id] = root;
        if (context.result.instance.root == UI_INVALID_HANDLE)
            context.result.instance.root = root;

        if (widget.hasSemantics)
            context.ui.setSemantics(context.surface, root, resolvedSemantics);
        applyNavigation(context, root, widget);
        if (widget.type != "Panel" && widget.type != "Button")
            applyDragDrop(context, root, widget);
        applySerializedBindings(context, root, progressFill, widget);

        for (const UiSerializedWidget& child : widget.children)
            instantiateWidget(context, childParent == UI_INVALID_HANDLE ? root : childParent, child);

        return root;
    }

    void resolveSemanticRelationships(InstantiationContext& context, const UiSerializedWidget& widget)
    {
        if (widget.hasSemantics && widget.hasSemanticRelationships && !widget.id.empty())
        {
            const auto handleIt = context.result.instance.handles.find(widget.id);
            if (handleIt != context.result.instance.handles.end())
            {
                UiSemanticDesc desc = widget.semantics;
                if (const UiAccessibilityManager* accessibility = context.ui.accessibilityManager(context.surface))
                {
                    if (const UiSemanticDesc* current = accessibility->semanticDesc(handleIt->second))
                        desc = *current;
                }
                if (!resolveSemanticRelationshipIds(context.result.instance.handles,
                                                    widget.semanticRelationships,
                                                    desc.relationships))
                {
                    context.result.validation.error(widget.id,
                                                    "semantic relationship references an unknown widget id");
                }
                context.ui.setSemantics(context.surface, handleIt->second, desc);
            }
        }

        for (const UiSerializedWidget& child : widget.children)
            resolveSemanticRelationships(context, child);
        for (const auto& [_, children] : widget.slots)
        {
            for (const UiSerializedWidget& child : children)
                resolveSemanticRelationships(context, child);
        }
    }
}

bool UiSerializedValidationResult::valid() const
{
    return std::none_of(issues.begin(),
                        issues.end(),
                        [](const UiSerializedValidationIssue& issue)
                        {
                            return issue.severity == UiSerializedValidationIssue::Severity::Error;
                        });
}

void UiSerializedValidationResult::error(std::string path, std::string message)
{
    issues.push_back({UiSerializedValidationIssue::Severity::Error,
                      std::move(path),
                      std::move(message)});
}

void UiSerializedValidationResult::warning(std::string path, std::string message)
{
    issues.push_back({UiSerializedValidationIssue::Severity::Warning,
                      std::move(path),
                      std::move(message)});
}

bool parseUiSerializedAsset(const std::string& json,
                            UiSerializedAsset& outAsset,
                            UiSerializedValidationResult* outValidation)
{
    UiSerializedValidationResult validation;
    GtsJsonValue root;
    std::string error;
    if (!GtsJsonParser::parse(json, root, &error))
    {
        validation.error("$", error);
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }
    if (!root.isObject())
    {
        validation.error("$", "UI asset root must be an object");
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    UiSerializedAsset asset;
    asset.schemaVersion = root.findInt32("schema").value_or(asset.schemaVersion);
    asset.id = root.findString("id").value_or(asset.id);
    asset.theme = root.findString("theme").value_or(asset.theme);
    if (const GtsJsonValue* surface = root.find("surface"))
        asset.surface = parseSurface(*surface);
    if (const GtsJsonValue* layers = root.find("layers"))
    {
        if (const Array* array = layers->tryArray())
        {
            for (const GtsJsonValue& item : *array)
                asset.layers.push_back(parseLayer(item));
        }
    }
    if (const GtsJsonValue* widget = root.find("root"))
        asset.root = parseWidget(*widget);
    else
        validation.error("$.root", "root widget is required");

    if (asset.schemaVersion < 1)
    {
        asset.schemaVersion = 1;
        validation.warning("$.schema", "migrated UI asset schema to version 1");
    }
    else if (asset.schemaVersion > UI_SERIALIZATION_SCHEMA_VERSION)
    {
        validation.error("$.schema", "unsupported future UI asset schema");
    }

    const UiSerializedValidationResult assetValidation = validateUiSerializedAsset(asset);
    validation.issues.insert(validation.issues.end(),
                             assetValidation.issues.begin(),
                             assetValidation.issues.end());
    outAsset = std::move(asset);
    if (outValidation != nullptr)
        *outValidation = validation;
    return validation.valid();
}

std::string serializeUiSerializedAsset(const UiSerializedAsset& asset)
{
    return GtsJsonParser::serialize(serializeAssetJson(asset), 0);
}

bool parseUiSerializedWidget(const GtsJsonValue& json, UiSerializedWidget& outWidget)
{
    outWidget = parseWidget(json);
    return !outWidget.type.empty() || !outWidget.asset.empty();
}

GtsJsonValue serializeUiSerializedWidget(const UiSerializedWidget& widget)
{
    return serializeWidget(widget);
}

bool loadUiSerializedAssetFromFile(const std::string& path,
                                   UiSerializedAsset& outAsset,
                                   UiSerializedValidationResult* outValidation)
{
    std::ifstream file(path);
    if (!file)
    {
        if (outValidation != nullptr)
            outValidation->error(path, "failed to open UI asset file");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseUiSerializedAsset(buffer.str(), outAsset, outValidation);
}

bool saveUiSerializedAssetToFile(const std::string& path, const UiSerializedAsset& asset, std::string* outError)
{
    std::string json;
    try
    {
        json = serializeUiSerializedAsset(asset);
    }
    catch (const std::invalid_argument& failure)
    {
        if (outError != nullptr)
            *outError = failure.what();
        return false;
    }
    std::ofstream file(path);
    if (!file)
    {
        if (outError != nullptr)
            *outError = "failed to open UI asset file for writing";
        return false;
    }
    file << json;
    if (!file.good())
    {
        if (outError != nullptr)
            *outError = "failed to write JSON file";
        return false;
    }
    if (outError != nullptr)
        outError->clear();
    return true;
}

UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset& asset, const UiTheme* theme)
{
    return validateUiSerializedAsset(asset, theme, nullptr);
}

UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset&     asset,
                                                       const UiTheme*               theme,
                                                       const UiWidgetAssetRegistry* widgetAssets)
{
    UiSerializedValidationResult result;
    if (asset.id.empty())
        result.warning("$.id", "asset id is empty");
    if (asset.schemaVersion != UI_SERIALIZATION_SCHEMA_VERSION)
        result.error("$.schema", "UI asset schema version is not supported");

    std::unordered_map<std::string, int> ids;
    std::vector<std::pair<std::string, std::string>> relationshipRefs;
    validateWidget(asset.root, theme, widgetAssets, result, "$.root", ids, relationshipRefs);
    for (const auto& [id, count] : ids)
    {
        if (count > 1)
            result.error("$.root", "duplicate widget id '" + id + "'");
    }
    for (const auto& [path, id] : relationshipRefs)
    {
        if (ids.find(id) == ids.end())
            result.error(path, "unknown semantic relationship widget id '" + id + "'");
    }
    return result;
}

UiSerializedLoadResult UiSerializationRuntime::instantiate(UiSystem& ui,
                                                           UiSurfaceId surface,
                                                           UiMountId mount,
                                                           const UiSerializedAsset& asset,
                                                           const IUiSerializedBindingResolver* bindingResolver,
                                                           const UiTheme* validationTheme,
                                                           const UiWidgetAssetRegistry* widgetAssets)
{
    UiSerializedLoadResult result;
    UiSerializedAsset resolvedAsset = asset;
    if (widgetAssets != nullptr)
    {
        if (!widgetAssets->expandSerializedAsset(asset, resolvedAsset, &result.validation))
            return result;
    }

    UiSerializedValidationResult validation =
        validateUiSerializedAsset(resolvedAsset, validationTheme, widgetAssets);
    result.validation.issues.insert(result.validation.issues.end(),
                                    validation.issues.begin(),
                                    validation.issues.end());
    if (!result.validation.valid())
        return result;

    UiHandle parent = ui.mountRoot(surface, mount);
    if (parent == UI_INVALID_HANDLE)
    {
        result.validation.error("$.mount", "invalid mount for serialized UI asset");
        return result;
    }

    InstantiationContext context{ui,
                                 surface,
                                 mount,
                                 bindingResolver,
                                 result,
                                 localizationScopeForAsset(resolvedAsset)};
    instantiateWidget(context, parent, resolvedAsset.root);
    resolveSemanticRelationships(context, resolvedAsset.root);
    result.success = result.validation.valid() && result.instance.root != UI_INVALID_HANDLE;
    return result;
}
