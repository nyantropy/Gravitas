#include "UiSerialization.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

#include "UiSystem.h"
#include "UiWidget.h"

namespace
{
    using Object = UiJsonValue::Object;
    using Array = UiJsonValue::Array;

    class JsonParser
    {
    public:
        explicit JsonParser(std::string_view inSource)
            : source(inSource)
        {
        }

        bool parse(UiJsonValue& value, std::string* outError)
        {
            skipWhitespace();
            if (!parseValue(value))
                return fail(outError);

            skipWhitespace();
            if (position != source.size())
            {
                error = "unexpected trailing content";
                return fail(outError);
            }
            return true;
        }

    private:
        std::string_view source;
        size_t position = 0;
        std::string error;

        bool fail(std::string* outError) const
        {
            if (outError != nullptr)
                *outError = error.empty() ? "invalid JSON" : error;
            return false;
        }

        void skipWhitespace()
        {
            while (position < source.size() &&
                   std::isspace(static_cast<unsigned char>(source[position])))
            {
                ++position;
            }
        }

        bool consume(char ch)
        {
            skipWhitespace();
            if (position >= source.size() || source[position] != ch)
                return false;

            ++position;
            return true;
        }

        bool parseValue(UiJsonValue& value)
        {
            skipWhitespace();
            if (position >= source.size())
            {
                error = "unexpected end of JSON";
                return false;
            }

            const char ch = source[position];
            if (ch == '{')
                return parseObject(value);
            if (ch == '[')
                return parseArray(value);
            if (ch == '"')
            {
                std::string string;
                if (!parseString(string))
                    return false;
                value.value = std::move(string);
                return true;
            }
            if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)))
                return parseNumber(value);
            if (source.substr(position, 4) == "true")
            {
                position += 4;
                value.value = true;
                return true;
            }
            if (source.substr(position, 5) == "false")
            {
                position += 5;
                value.value = false;
                return true;
            }
            if (source.substr(position, 4) == "null")
            {
                position += 4;
                value.value = std::monostate{};
                return true;
            }

            error = "unexpected JSON token";
            return false;
        }

        bool parseObject(UiJsonValue& value)
        {
            if (!consume('{'))
                return false;

            Object object;
            skipWhitespace();
            if (consume('}'))
            {
                value.value = std::move(object);
                return true;
            }

            while (position < source.size())
            {
                std::string key;
                if (!parseString(key))
                    return false;
                if (!consume(':'))
                {
                    error = "expected ':' after object key";
                    return false;
                }

                UiJsonValue member;
                if (!parseValue(member))
                    return false;
                object.emplace_back(std::move(key), std::move(member));

                skipWhitespace();
                if (consume('}'))
                {
                    value.value = std::move(object);
                    return true;
                }
                if (!consume(','))
                {
                    error = "expected ',' or '}' in object";
                    return false;
                }
            }

            error = "unterminated object";
            return false;
        }

        bool parseArray(UiJsonValue& value)
        {
            if (!consume('['))
                return false;

            Array array;
            skipWhitespace();
            if (consume(']'))
            {
                value.value = std::move(array);
                return true;
            }

            while (position < source.size())
            {
                UiJsonValue item;
                if (!parseValue(item))
                    return false;
                array.push_back(std::move(item));

                skipWhitespace();
                if (consume(']'))
                {
                    value.value = std::move(array);
                    return true;
                }
                if (!consume(','))
                {
                    error = "expected ',' or ']' in array";
                    return false;
                }
            }

            error = "unterminated array";
            return false;
        }

        bool parseString(std::string& value)
        {
            skipWhitespace();
            if (position >= source.size() || source[position] != '"')
            {
                error = "expected string";
                return false;
            }

            ++position;
            value.clear();
            while (position < source.size())
            {
                const char ch = source[position++];
                if (ch == '"')
                    return true;

                if (ch == '\\')
                {
                    if (position >= source.size())
                    {
                        error = "unterminated escape sequence";
                        return false;
                    }

                    const char escaped = source[position++];
                    switch (escaped)
                    {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case '/': value.push_back('/'); break;
                        case 'b': value.push_back('\b'); break;
                        case 'f': value.push_back('\f'); break;
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        default:
                            error = "unsupported escape sequence";
                            return false;
                    }
                    continue;
                }

                value.push_back(ch);
            }

            error = "unterminated string";
            return false;
        }

        bool parseNumber(UiJsonValue& value)
        {
            size_t end = position;
            if (source[end] == '-')
                ++end;
            while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
                ++end;
            if (end < source.size() && source[end] == '.')
            {
                ++end;
                while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
                    ++end;
            }
            if (end < source.size() && (source[end] == 'e' || source[end] == 'E'))
            {
                ++end;
                if (end < source.size() && (source[end] == '+' || source[end] == '-'))
                    ++end;
                while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
                    ++end;
            }
            if (end == position)
            {
                error = "expected number";
                return false;
            }

            try
            {
                value.value = std::stod(std::string(source.substr(position, end - position)));
            }
            catch (...)
            {
                error = "invalid number";
                return false;
            }
            position = end;
            return true;
        }
    };

    std::string escapeJson(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 2);
        for (char ch : value)
        {
            switch (ch)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(ch); break;
            }
        }
        return out;
    }

    std::string indentString(int indent)
    {
        return std::string(static_cast<size_t>(std::max(0, indent)), ' ');
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

    UiJsonValue jsonObject(Object object)
    {
        UiJsonValue json;
        json.value = std::move(object);
        return json;
    }

    UiJsonValue jsonArray(Array array)
    {
        UiJsonValue json;
        json.value = std::move(array);
        return json;
    }

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

    float numberOr(const UiJsonValue& object, const std::string& key, float fallback)
    {
        const auto value = getNumber(object, key);
        return value.has_value() ? static_cast<float>(*value) : fallback;
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

    std::string stringOr(const UiJsonValue& object, const std::string& key, const std::string& fallback = {})
    {
        const auto value = getString(object, key);
        return value.value_or(fallback);
    }

    UiVec2 parseVec2(const UiJsonValue& value, UiVec2 fallback)
    {
        const Array* array = asArray(value);
        if (array == nullptr || array->size() < 2)
            return fallback;

        const auto* x = std::get_if<double>(&(*array)[0].value);
        const auto* y = std::get_if<double>(&(*array)[1].value);
        return x != nullptr && y != nullptr
            ? UiVec2{static_cast<float>(*x), static_cast<float>(*y)}
            : fallback;
    }

    UiRect parseRect(const UiJsonValue& value, UiRect fallback)
    {
        const Array* array = asArray(value);
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto* x = std::get_if<double>(&(*array)[0].value);
        const auto* y = std::get_if<double>(&(*array)[1].value);
        const auto* w = std::get_if<double>(&(*array)[2].value);
        const auto* h = std::get_if<double>(&(*array)[3].value);
        return x != nullptr && y != nullptr && w != nullptr && h != nullptr
            ? UiRect{static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*w), static_cast<float>(*h)}
            : fallback;
    }

    UiThickness parseThickness(const UiJsonValue& value, UiThickness fallback)
    {
        const Array* array = asArray(value);
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto* l = std::get_if<double>(&(*array)[0].value);
        const auto* t = std::get_if<double>(&(*array)[1].value);
        const auto* r = std::get_if<double>(&(*array)[2].value);
        const auto* b = std::get_if<double>(&(*array)[3].value);
        return l != nullptr && t != nullptr && r != nullptr && b != nullptr
            ? UiThickness{static_cast<float>(*l), static_cast<float>(*t), static_cast<float>(*r), static_cast<float>(*b)}
            : fallback;
    }

    UiColor parseColor(const UiJsonValue& value, UiColor fallback)
    {
        const Array* array = asArray(value);
        if (array == nullptr || array->size() < 4)
            return fallback;

        const auto* r = std::get_if<double>(&(*array)[0].value);
        const auto* g = std::get_if<double>(&(*array)[1].value);
        const auto* b = std::get_if<double>(&(*array)[2].value);
        const auto* a = std::get_if<double>(&(*array)[3].value);
        return r != nullptr && g != nullptr && b != nullptr && a != nullptr
            ? UiColor{static_cast<float>(*r), static_cast<float>(*g), static_cast<float>(*b), static_cast<float>(*a)}
            : fallback;
    }

    UiJsonValue serializeVec2(UiVec2 value)
    {
        return jsonArray({jsonNumber(value.x), jsonNumber(value.y)});
    }

    UiJsonValue serializeRect(UiRect value)
    {
        return jsonArray({jsonNumber(value.x), jsonNumber(value.y), jsonNumber(value.width), jsonNumber(value.height)});
    }

    UiJsonValue serializeThickness(UiThickness value)
    {
        return jsonArray({jsonNumber(value.left), jsonNumber(value.top), jsonNumber(value.right), jsonNumber(value.bottom)});
    }

    UiJsonValue serializeColor(UiColor value)
    {
        return jsonArray({jsonNumber(value.r), jsonNumber(value.g), jsonNumber(value.b), jsonNumber(value.a)});
    }

    template <typename Enum>
    Enum parseEnumString(const std::string& value,
                         const std::vector<std::pair<std::string, Enum>>& entries,
                         Enum fallback)
    {
        for (const auto& [name, enumValue] : entries)
        {
            if (name == value)
                return enumValue;
        }
        return fallback;
    }

    template <typename Enum>
    std::string enumToString(Enum value,
                             const std::vector<std::pair<std::string, Enum>>& entries,
                             const std::string& fallback)
    {
        for (const auto& [name, enumValue] : entries)
        {
            if (enumValue == value)
                return name;
        }
        return fallback;
    }

    const std::vector<std::pair<std::string, UiLayoutMode>>& layoutModeEntries()
    {
        static const std::vector<std::pair<std::string, UiLayoutMode>> entries = {
            {"Canvas", UiLayoutMode::Canvas}, {"Stack", UiLayoutMode::Stack},
            {"Grid", UiLayoutMode::Grid}, {"Dock", UiLayoutMode::Dock},
            {"Overlay", UiLayoutMode::Overlay}, {"Scroll", UiLayoutMode::Scroll},
            {"Aspect", UiLayoutMode::Aspect}, {"Constraint", UiLayoutMode::Constraint}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiPositionMode>>& positionModeEntries()
    {
        static const std::vector<std::pair<std::string, UiPositionMode>> entries = {
            {"Absolute", UiPositionMode::Absolute}, {"Anchored", UiPositionMode::Anchored}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiSizeMode>>& sizeModeEntries()
    {
        static const std::vector<std::pair<std::string, UiSizeMode>> entries = {
            {"FromAnchors", UiSizeMode::FromAnchors}, {"Fixed", UiSizeMode::Fixed}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiClipMode>>& clipModeEntries()
    {
        static const std::vector<std::pair<std::string, UiClipMode>> entries = {
            {"None", UiClipMode::None}, {"ClipChildren", UiClipMode::ClipChildren}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiHorizontalAlign>>& horizontalAlignEntries()
    {
        static const std::vector<std::pair<std::string, UiHorizontalAlign>> entries = {
            {"Left", UiHorizontalAlign::Left}, {"Center", UiHorizontalAlign::Center},
            {"Right", UiHorizontalAlign::Right}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiVerticalAlign>>& verticalAlignEntries()
    {
        static const std::vector<std::pair<std::string, UiVerticalAlign>> entries = {
            {"Top", UiVerticalAlign::Top}, {"Middle", UiVerticalAlign::Middle},
            {"Bottom", UiVerticalAlign::Bottom}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiTextWrapMode>>& wrapModeEntries()
    {
        static const std::vector<std::pair<std::string, UiTextWrapMode>> entries = {
            {"None", UiTextWrapMode::None}, {"Word", UiTextWrapMode::Word}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiLayoutAxis>>& axisEntries()
    {
        static const std::vector<std::pair<std::string, UiLayoutAxis>> entries = {
            {"Horizontal", UiLayoutAxis::Horizontal}, {"Vertical", UiLayoutAxis::Vertical}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiLayoutAlignment>>& alignmentEntries()
    {
        static const std::vector<std::pair<std::string, UiLayoutAlignment>> entries = {
            {"Start", UiLayoutAlignment::Start}, {"Center", UiLayoutAlignment::Center},
            {"End", UiLayoutAlignment::End}, {"Stretch", UiLayoutAlignment::Stretch}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiLayoutUnit>>& unitEntries()
    {
        static const std::vector<std::pair<std::string, UiLayoutUnit>> entries = {
            {"Auto", UiLayoutUnit::Auto}, {"Normalized", UiLayoutUnit::Normalized},
            {"Percent", UiLayoutUnit::Percent}, {"SurfaceWidth", UiLayoutUnit::SurfaceWidth},
            {"SurfaceHeight", UiLayoutUnit::SurfaceHeight}, {"ParentWidth", UiLayoutUnit::ParentWidth},
            {"ParentHeight", UiLayoutUnit::ParentHeight}, {"Content", UiLayoutUnit::Content},
            {"Em", UiLayoutUnit::Em}, {"Pixels", UiLayoutUnit::Pixels}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiDockEdge>>& dockEntries()
    {
        static const std::vector<std::pair<std::string, UiDockEdge>> entries = {
            {"Left", UiDockEdge::Left}, {"Right", UiDockEdge::Right},
            {"Top", UiDockEdge::Top}, {"Bottom", UiDockEdge::Bottom}, {"Fill", UiDockEdge::Fill}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiBindableProperty>>& bindingPropertyEntries()
    {
        static const std::vector<std::pair<std::string, UiBindableProperty>> entries = {
            {"Text", UiBindableProperty::Text}, {"Visible", UiBindableProperty::Visible},
            {"Enabled", UiBindableProperty::Enabled}, {"Interactable", UiBindableProperty::Interactable},
            {"Opacity", UiBindableProperty::Opacity}, {"Progress", UiBindableProperty::Progress},
            {"RectColor", UiBindableProperty::RectColor}, {"TextColor", UiBindableProperty::TextColor},
            {"ImageAsset", UiBindableProperty::ImageAsset}, {"ImageTint", UiBindableProperty::ImageTint},
            {"StyleClass", UiBindableProperty::StyleClass}, {"Layout", UiBindableProperty::Layout},
            {"LayoutOffsetMin", UiBindableProperty::LayoutOffsetMin},
            {"LayoutOffsetMax", UiBindableProperty::LayoutOffsetMax},
            {"LayoutAnchorMin", UiBindableProperty::LayoutAnchorMin},
            {"LayoutAnchorMax", UiBindableProperty::LayoutAnchorMax},
            {"LayoutFixedSize", UiBindableProperty::LayoutFixedSize},
            {"LayoutContentOffset", UiBindableProperty::LayoutContentOffset}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiNavigationRole>>& navigationRoleEntries()
    {
        static const std::vector<std::pair<std::string, UiNavigationRole>> entries = {
            {"Generic", UiNavigationRole::Generic}, {"Button", UiNavigationRole::Button},
            {"Checkbox", UiNavigationRole::Checkbox}, {"Slider", UiNavigationRole::Slider},
            {"List", UiNavigationRole::List}, {"Tree", UiNavigationRole::Tree},
            {"TextBox", UiNavigationRole::TextBox}, {"Menu", UiNavigationRole::Menu},
            {"Tab", UiNavigationRole::Tab}, {"Window", UiNavigationRole::Window},
            {"Toolbar", UiNavigationRole::Toolbar}, {"Graph", UiNavigationRole::Graph},
            {"Viewport", UiNavigationRole::Viewport}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiNavigationDirection>>& navigationDirectionEntries()
    {
        static const std::vector<std::pair<std::string, UiNavigationDirection>> entries = {
            {"Up", UiNavigationDirection::Up}, {"Down", UiNavigationDirection::Down},
            {"Left", UiNavigationDirection::Left}, {"Right", UiNavigationDirection::Right},
            {"Next", UiNavigationDirection::Next}, {"Previous", UiNavigationDirection::Previous}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiSemanticRole>>& semanticRoleEntries()
    {
        static const std::vector<std::pair<std::string, UiSemanticRole>> entries = {
            {"Unknown", UiSemanticRole::Unknown}, {"Window", UiSemanticRole::Window},
            {"Dialog", UiSemanticRole::Dialog}, {"Panel", UiSemanticRole::Panel},
            {"Button", UiSemanticRole::Button}, {"Toggle", UiSemanticRole::Toggle},
            {"Checkbox", UiSemanticRole::Checkbox}, {"Radio", UiSemanticRole::Radio},
            {"Slider", UiSemanticRole::Slider}, {"Textbox", UiSemanticRole::Textbox},
            {"Password", UiSemanticRole::Password}, {"Image", UiSemanticRole::Image},
            {"Label", UiSemanticRole::Label}, {"Heading", UiSemanticRole::Heading},
            {"Group", UiSemanticRole::Group}, {"List", UiSemanticRole::List},
            {"ListItem", UiSemanticRole::ListItem}, {"Tree", UiSemanticRole::Tree},
            {"TreeItem", UiSemanticRole::TreeItem}, {"Table", UiSemanticRole::Table},
            {"Row", UiSemanticRole::Row}, {"Cell", UiSemanticRole::Cell},
            {"ProgressBar", UiSemanticRole::ProgressBar}, {"Status", UiSemanticRole::Status},
            {"Toolbar", UiSemanticRole::Toolbar}, {"Menu", UiSemanticRole::Menu},
            {"MenuItem", UiSemanticRole::MenuItem}, {"Tab", UiSemanticRole::Tab},
            {"TabPanel", UiSemanticRole::TabPanel}, {"Viewport", UiSemanticRole::Viewport},
            {"Canvas", UiSemanticRole::Canvas}, {"Graph", UiSemanticRole::Graph},
            {"Inspector", UiSemanticRole::Inspector}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiAccessibilityLiveRegion>>& liveRegionEntries()
    {
        static const std::vector<std::pair<std::string, UiAccessibilityLiveRegion>> entries = {
            {"Off", UiAccessibilityLiveRegion::Off}, {"Polite", UiAccessibilityLiveRegion::Polite},
            {"Assertive", UiAccessibilityLiveRegion::Assertive}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, UiSurfaceKind>>& surfaceKindEntries()
    {
        static const std::vector<std::pair<std::string, UiSurfaceKind>> entries = {
            {"Screen", UiSurfaceKind::Screen}, {"Viewport", UiSurfaceKind::Viewport},
            {"RenderTarget", UiSurfaceKind::RenderTarget}, {"World", UiSurfaceKind::World},
            {"Window", UiSurfaceKind::Window}, {"Custom", UiSurfaceKind::Custom}
        };
        return entries;
    }

    const std::vector<std::pair<std::string, gts::tween::TweenEase>>& easeEntries()
    {
        static const std::vector<std::pair<std::string, gts::tween::TweenEase>> entries = {
            {"Linear", gts::tween::TweenEase::Linear},
            {"EaseInQuad", gts::tween::TweenEase::EaseInQuad},
            {"EaseOutQuad", gts::tween::TweenEase::EaseOutQuad},
            {"EaseInOutQuad", gts::tween::TweenEase::EaseInOutQuad},
            {"SmoothStep", gts::tween::TweenEase::SmoothStep}
        };
        return entries;
    }

    UiLayoutLength parseLength(const UiJsonValue& value, UiLayoutLength fallback)
    {
        const Object* object = asObject(value);
        if (object == nullptr)
            return fallback;

        UiLayoutLength length = fallback;
        if (const auto unit = getString(value, "unit"))
            length.unit = parseEnumString(*unit, unitEntries(), length.unit);
        if (const auto amount = getNumber(value, "value"))
            length.value = static_cast<float>(*amount);
        return length;
    }

    UiJsonValue serializeLength(const UiLayoutLength& length)
    {
        return jsonObject({
            {"unit", jsonString(enumToString(length.unit, unitEntries(), "Auto"))},
            {"value", jsonNumber(length.value)}
        });
    }

    void parseLayoutConstraints(const UiJsonValue& json, UiLayoutConstraints& constraints)
    {
        if (const UiJsonValue* value = json.find("minWidth")) constraints.minWidth = parseLength(*value, constraints.minWidth);
        if (const UiJsonValue* value = json.find("minHeight")) constraints.minHeight = parseLength(*value, constraints.minHeight);
        if (const UiJsonValue* value = json.find("maxWidth")) constraints.maxWidth = parseLength(*value, constraints.maxWidth);
        if (const UiJsonValue* value = json.find("maxHeight")) constraints.maxHeight = parseLength(*value, constraints.maxHeight);
        if (const UiJsonValue* value = json.find("preferredWidth")) constraints.preferredWidth = parseLength(*value, constraints.preferredWidth);
        if (const UiJsonValue* value = json.find("preferredHeight")) constraints.preferredHeight = parseLength(*value, constraints.preferredHeight);
        constraints.grow = numberOr(json, "grow", constraints.grow);
        constraints.shrink = numberOr(json, "shrink", constraints.shrink);
        constraints.aspectRatio = numberOr(json, "aspectRatio", constraints.aspectRatio);
        if (const auto value = getString(json, "horizontalAlignment"))
            constraints.horizontalAlignment = parseEnumString(*value, alignmentEntries(), constraints.horizontalAlignment);
        if (const auto value = getString(json, "verticalAlignment"))
            constraints.verticalAlignment = parseEnumString(*value, alignmentEntries(), constraints.verticalAlignment);
    }

    UiJsonValue serializeConstraints(const UiLayoutConstraints& constraints)
    {
        return jsonObject({
            {"minWidth", serializeLength(constraints.minWidth)},
            {"minHeight", serializeLength(constraints.minHeight)},
            {"maxWidth", serializeLength(constraints.maxWidth)},
            {"maxHeight", serializeLength(constraints.maxHeight)},
            {"preferredWidth", serializeLength(constraints.preferredWidth)},
            {"preferredHeight", serializeLength(constraints.preferredHeight)},
            {"grow", jsonNumber(constraints.grow)},
            {"shrink", jsonNumber(constraints.shrink)},
            {"aspectRatio", jsonNumber(constraints.aspectRatio)},
            {"horizontalAlignment", jsonString(enumToString(constraints.horizontalAlignment, alignmentEntries(), "Stretch"))},
            {"verticalAlignment", jsonString(enumToString(constraints.verticalAlignment, alignmentEntries(), "Stretch"))}
        });
    }

    UiLayoutSpec parseLayout(const UiJsonValue& json, UiLayoutSpec layout = {})
    {
        if (!json.isObject())
            return layout;

        if (const auto value = getString(json, "mode"))
            layout.layoutMode = parseEnumString(*value, layoutModeEntries(), layout.layoutMode);
        if (const auto value = getString(json, "position"))
            layout.positionMode = parseEnumString(*value, positionModeEntries(), layout.positionMode);
        if (const auto value = getString(json, "width"))
            layout.widthMode = parseEnumString(*value, sizeModeEntries(), layout.widthMode);
        if (const auto value = getString(json, "height"))
            layout.heightMode = parseEnumString(*value, sizeModeEntries(), layout.heightMode);
        if (const UiJsonValue* value = json.find("anchorMin")) layout.anchorMin = parseVec2(*value, layout.anchorMin);
        if (const UiJsonValue* value = json.find("anchorMax")) layout.anchorMax = parseVec2(*value, layout.anchorMax);
        if (const UiJsonValue* value = json.find("offsetMin")) layout.offsetMin = parseVec2(*value, layout.offsetMin);
        if (const UiJsonValue* value = json.find("offsetMax")) layout.offsetMax = parseVec2(*value, layout.offsetMax);
        if (const UiJsonValue* value = json.find("fixedSize"))
        {
            const UiVec2 size = parseVec2(*value, {layout.fixedWidth, layout.fixedHeight});
            layout.fixedWidth = size.x;
            layout.fixedHeight = size.y;
        }
        layout.fixedWidth = numberOr(json, "fixedWidth", layout.fixedWidth);
        layout.fixedHeight = numberOr(json, "fixedHeight", layout.fixedHeight);
        if (const UiJsonValue* value = json.find("margin")) layout.margin = parseThickness(*value, layout.margin);
        if (const UiJsonValue* value = json.find("padding")) layout.padding = parseThickness(*value, layout.padding);
        if (const auto value = getString(json, "clip"))
            layout.clipMode = parseEnumString(*value, clipModeEntries(), layout.clipMode);
        if (const UiJsonValue* value = json.find("contentOffset")) layout.contentOffset = parseVec2(*value, layout.contentOffset);
        layout.gap = numberOr(json, "gap", layout.gap);
        if (const auto value = getString(json, "stackAxis"))
            layout.stackAxis = parseEnumString(*value, axisEntries(), layout.stackAxis);
        if (const auto value = getString(json, "mainAxisAlignment"))
            layout.mainAxisAlignment = parseEnumString(*value, alignmentEntries(), layout.mainAxisAlignment);
        if (const auto value = getString(json, "crossAxisAlignment"))
            layout.crossAxisAlignment = parseEnumString(*value, alignmentEntries(), layout.crossAxisAlignment);
        layout.gridColumns = intOr(json, "gridColumns", layout.gridColumns);
        layout.gridRows = intOr(json, "gridRows", layout.gridRows);
        layout.gridColumnGap = numberOr(json, "gridColumnGap", layout.gridColumnGap);
        layout.gridRowGap = numberOr(json, "gridRowGap", layout.gridRowGap);
        layout.gridColumn = intOr(json, "gridColumn", layout.gridColumn);
        layout.gridRow = intOr(json, "gridRow", layout.gridRow);
        layout.gridColumnSpan = intOr(json, "gridColumnSpan", layout.gridColumnSpan);
        layout.gridRowSpan = intOr(json, "gridRowSpan", layout.gridRowSpan);
        if (const auto value = getString(json, "dock"))
            layout.dock = parseEnumString(*value, dockEntries(), layout.dock);
        if (const UiJsonValue* value = json.find("constraints"))
            parseLayoutConstraints(*value, layout.constraints);
        return layout;
    }

    UiJsonValue serializeLayout(const UiLayoutSpec& layout)
    {
        return jsonObject({
            {"mode", jsonString(enumToString(layout.layoutMode, layoutModeEntries(), "Canvas"))},
            {"position", jsonString(enumToString(layout.positionMode, positionModeEntries(), "Absolute"))},
            {"width", jsonString(enumToString(layout.widthMode, sizeModeEntries(), "Fixed"))},
            {"height", jsonString(enumToString(layout.heightMode, sizeModeEntries(), "Fixed"))},
            {"anchorMin", serializeVec2(layout.anchorMin)},
            {"anchorMax", serializeVec2(layout.anchorMax)},
            {"offsetMin", serializeVec2(layout.offsetMin)},
            {"offsetMax", serializeVec2(layout.offsetMax)},
            {"fixedSize", serializeVec2({layout.fixedWidth, layout.fixedHeight})},
            {"margin", serializeThickness(layout.margin)},
            {"padding", serializeThickness(layout.padding)},
            {"clip", jsonString(enumToString(layout.clipMode, clipModeEntries(), "None"))},
            {"contentOffset", serializeVec2(layout.contentOffset)},
            {"gap", jsonNumber(layout.gap)},
            {"stackAxis", jsonString(enumToString(layout.stackAxis, axisEntries(), "Vertical"))},
            {"mainAxisAlignment", jsonString(enumToString(layout.mainAxisAlignment, alignmentEntries(), "Start"))},
            {"crossAxisAlignment", jsonString(enumToString(layout.crossAxisAlignment, alignmentEntries(), "Stretch"))},
            {"gridColumns", jsonNumber(layout.gridColumns)},
            {"gridRows", jsonNumber(layout.gridRows)},
            {"gridColumnGap", jsonNumber(layout.gridColumnGap)},
            {"gridRowGap", jsonNumber(layout.gridRowGap)},
            {"gridColumn", jsonNumber(layout.gridColumn)},
            {"gridRow", jsonNumber(layout.gridRow)},
            {"gridColumnSpan", jsonNumber(layout.gridColumnSpan)},
            {"gridRowSpan", jsonNumber(layout.gridRowSpan)},
            {"dock", jsonString(enumToString(layout.dock, dockEntries(), "Fill"))},
            {"constraints", serializeConstraints(layout.constraints)}
        });
    }

    UiAnimationTiming parseTiming(const UiJsonValue& json, UiAnimationTiming timing = {})
    {
        if (!json.isObject())
            return timing;
        timing.durationSeconds = numberOr(json, "duration", timing.durationSeconds);
        timing.delaySeconds = numberOr(json, "delay", timing.delaySeconds);
        if (const auto value = getString(json, "ease"))
            timing.ease = parseEnumString(*value, easeEntries(), timing.ease);
        timing.repeatCount = static_cast<uint32_t>(intOr(json, "repeat", static_cast<int>(timing.repeatCount)));
        timing.loop = boolOr(json, "loop", timing.loop);
        timing.pingPong = boolOr(json, "pingPong", timing.pingPong);
        timing.snapToEnd = boolOr(json, "snapToEnd", timing.snapToEnd);
        return timing;
    }

    UiJsonValue serializeTiming(const UiAnimationTiming& timing)
    {
        return jsonObject({
            {"duration", jsonNumber(timing.durationSeconds)},
            {"delay", jsonNumber(timing.delaySeconds)},
            {"ease", jsonString(enumToString(timing.ease, easeEntries(), "SmoothStep"))},
            {"repeat", jsonNumber(timing.repeatCount)},
            {"loop", jsonBool(timing.loop)},
            {"pingPong", jsonBool(timing.pingPong)},
            {"snapToEnd", jsonBool(timing.snapToEnd)}
        });
    }

    UiSemanticRelationship parseRelationship(const UiJsonValue& json)
    {
        UiSemanticRelationship relationship;
        const auto parseHandleIds = [&](const char* key, std::vector<UiHandle>& target)
        {
            const UiJsonValue* value = json.find(key);
            const Array* array = value == nullptr ? nullptr : asArray(*value);
            if (array == nullptr)
                return;
            for (const UiJsonValue& item : *array)
            {
                if (const auto* number = std::get_if<double>(&item.value))
                    target.push_back(static_cast<UiHandle>(*number));
            }
        };
        parseHandleIds("labelledBy", relationship.labelledBy);
        parseHandleIds("describedBy", relationship.describedBy);
        parseHandleIds("controls", relationship.controls);
        parseHandleIds("owns", relationship.owns);
        relationship.activeDescendant = static_cast<UiHandle>(intOr(json, "activeDescendant", relationship.activeDescendant));
        relationship.popup = static_cast<UiHandle>(intOr(json, "popup", relationship.popup));
        relationship.tooltip = static_cast<UiHandle>(intOr(json, "tooltip", relationship.tooltip));
        return relationship;
    }

    UiJsonValue serializeHandleArray(const std::vector<UiHandle>& values)
    {
        Array array;
        for (UiHandle handle : values)
            array.push_back(jsonNumber(handle));
        return jsonArray(std::move(array));
    }

    UiJsonValue serializeRelationship(const UiSemanticRelationship& relationship)
    {
        return jsonObject({
            {"labelledBy", serializeHandleArray(relationship.labelledBy)},
            {"describedBy", serializeHandleArray(relationship.describedBy)},
            {"controls", serializeHandleArray(relationship.controls)},
            {"owns", serializeHandleArray(relationship.owns)},
            {"activeDescendant", jsonNumber(relationship.activeDescendant)},
            {"popup", jsonNumber(relationship.popup)},
            {"tooltip", jsonNumber(relationship.tooltip)}
        });
    }

    UiSemanticDesc parseSemantic(const UiJsonValue& json)
    {
        UiSemanticDesc semantic;
        if (!json.isObject())
            return semantic;

        if (const auto value = getString(json, "role"))
            semantic.role = parseEnumString(*value, semanticRoleEntries(), semantic.role);
        semantic.name = stringOr(json, "name", semantic.name);
        semantic.description = stringOr(json, "description", semantic.description);
        semantic.hint = stringOr(json, "hint", semantic.hint);
        semantic.value = stringOr(json, "value", semantic.value);
        if (const auto value = getString(json, "liveRegion"))
            semantic.liveRegion = parseEnumString(*value, liveRegionEntries(), semantic.liveRegion);
        semantic.hidden = boolOr(json, "hidden", semantic.hidden);
        semantic.decorative = boolOr(json, "decorative", semantic.decorative);
        semantic.selected = boolOr(json, "selected", semantic.selected);
        semantic.checked = boolOr(json, "checked", semantic.checked);
        semantic.expanded = boolOr(json, "expanded", semantic.expanded);
        semantic.readOnly = boolOr(json, "readOnly", semantic.readOnly);
        semantic.busy = boolOr(json, "busy", semantic.busy);
        semantic.hasRange = boolOr(json, "hasRange", semantic.hasRange);
        semantic.rangeMin = numberOr(json, "rangeMin", semantic.rangeMin);
        semantic.rangeMax = numberOr(json, "rangeMax", semantic.rangeMax);
        semantic.rangeValue = numberOr(json, "rangeValue", semantic.rangeValue);
        semantic.level = intOr(json, "level", semantic.level);
        semantic.index = intOr(json, "index", semantic.index);
        semantic.count = intOr(json, "count", semantic.count);
        if (const UiJsonValue* value = json.find("relationships"))
            semantic.relationships = parseRelationship(*value);
        return semantic;
    }

    UiJsonValue serializeSemantic(const UiSemanticDesc& semantic)
    {
        return jsonObject({
            {"role", jsonString(enumToString(semantic.role, semanticRoleEntries(), "Unknown"))},
            {"name", jsonString(semantic.name)},
            {"description", jsonString(semantic.description)},
            {"hint", jsonString(semantic.hint)},
            {"value", jsonString(semantic.value)},
            {"liveRegion", jsonString(enumToString(semantic.liveRegion, liveRegionEntries(), "Off"))},
            {"relationships", serializeRelationship(semantic.relationships)},
            {"hidden", jsonBool(semantic.hidden)},
            {"decorative", jsonBool(semantic.decorative)},
            {"selected", jsonBool(semantic.selected)},
            {"checked", jsonBool(semantic.checked)},
            {"expanded", jsonBool(semantic.expanded)},
            {"readOnly", jsonBool(semantic.readOnly)},
            {"busy", jsonBool(semantic.busy)},
            {"hasRange", jsonBool(semantic.hasRange)},
            {"rangeMin", jsonNumber(semantic.rangeMin)},
            {"rangeMax", jsonNumber(semantic.rangeMax)},
            {"rangeValue", jsonNumber(semantic.rangeValue)},
            {"level", jsonNumber(semantic.level)},
            {"index", jsonNumber(semantic.index)},
            {"count", jsonNumber(semantic.count)}
        });
    }

    UiSerializedBinding parseBinding(const UiJsonValue& json)
    {
        UiSerializedBinding binding;
        if (!json.isObject())
            return binding;
        if (const auto value = getString(json, "property"))
            binding.property = parseEnumString(*value, bindingPropertyEntries(), binding.property);
        binding.path = stringOr(json, "path", binding.path);
        binding.formatter = stringOr(json, "formatter", binding.formatter);
        binding.transform = stringOr(json, "transform", binding.transform);
        binding.animateInitial = boolOr(json, "animateInitial", binding.animateInitial);
        binding.applyImmediately = boolOr(json, "applyImmediately", binding.applyImmediately);
        if (const UiJsonValue* value = json.find("animation"))
            binding.animation = parseTiming(*value);
        return binding;
    }

    UiJsonValue serializeBinding(const UiSerializedBinding& binding)
    {
        Object object = {
            {"property", jsonString(enumToString(binding.property, bindingPropertyEntries(), "Text"))},
            {"path", jsonString(binding.path)},
            {"formatter", jsonString(binding.formatter)},
            {"transform", jsonString(binding.transform)},
            {"animateInitial", jsonBool(binding.animateInitial)},
            {"applyImmediately", jsonBool(binding.applyImmediately)}
        };
        if (binding.animation)
            object.emplace_back("animation", serializeTiming(*binding.animation));
        return jsonObject(std::move(object));
    }

    UiSerializedNavigation parseNavigation(const UiJsonValue& json)
    {
        UiSerializedNavigation navigation;
        if (!json.isObject())
            return navigation;
        navigation.enabled = true;
        navigation.focusable = boolOr(json, "focusable", navigation.focusable);
        navigation.enabled = boolOr(json, "enabled", navigation.enabled);
        if (const auto value = getString(json, "role"))
            navigation.role = parseEnumString(*value, navigationRoleEntries(), navigation.role);
        navigation.scope = static_cast<uint32_t>(intOr(json, "scope", static_cast<int>(navigation.scope)));
        navigation.group = stringOr(json, "group", navigation.group);
        navigation.tabIndex = intOr(json, "tabIndex", navigation.tabIndex);
        navigation.wrapNavigation = boolOr(json, "wrapNavigation", navigation.wrapNavigation);
        navigation.activateOnSubmit = boolOr(json, "activateOnSubmit", navigation.activateOnSubmit);
        if (const UiJsonValue* neighbors = json.find("neighbors"))
        {
            if (const Object* object = asObject(*neighbors))
            {
                for (const auto& [key, value] : *object)
                {
                    if (const auto* string = std::get_if<std::string>(&value.value))
                    {
                        const UiNavigationDirection direction =
                            parseEnumString(key, navigationDirectionEntries(), UiNavigationDirection::None);
                        if (direction != UiNavigationDirection::None)
                            navigation.neighbors[direction] = *string;
                    }
                }
            }
        }
        return navigation;
    }

    UiJsonValue serializeNavigation(const UiSerializedNavigation& navigation)
    {
        Object neighbors;
        for (const auto& [direction, target] : navigation.neighbors)
            neighbors.emplace_back(enumToString(direction, navigationDirectionEntries(), "None"), jsonString(target));

        return jsonObject({
            {"enabled", jsonBool(navigation.enabled)},
            {"focusable", jsonBool(navigation.focusable)},
            {"role", jsonString(enumToString(navigation.role, navigationRoleEntries(), "Generic"))},
            {"scope", jsonNumber(navigation.scope)},
            {"group", jsonString(navigation.group)},
            {"tabIndex", jsonNumber(navigation.tabIndex)},
            {"wrapNavigation", jsonBool(navigation.wrapNavigation)},
            {"activateOnSubmit", jsonBool(navigation.activateOnSubmit)},
            {"neighbors", jsonObject(std::move(neighbors))}
        });
    }

    UiSerializedDragSource parseDragSource(const UiJsonValue& json)
    {
        UiSerializedDragSource source;
        if (!json.isObject())
            return source;
        source.enabled = boolOr(json, "enabled", source.enabled);
        source.payloadType = stringOr(json, "payloadType", source.payloadType);
        source.payloadId = static_cast<uint64_t>(std::max(0, intOr(json, "payloadId", static_cast<int>(source.payloadId))));
        source.payloadLabel = stringOr(json, "payloadLabel", source.payloadLabel);
        source.startThreshold = numberOr(json, "startThreshold", source.startThreshold);
        source.capturePointer = boolOr(json, "capturePointer", source.capturePointer);
        return source;
    }

    UiJsonValue serializeDragSource(const UiSerializedDragSource& source)
    {
        return jsonObject({
            {"enabled", jsonBool(source.enabled)},
            {"payloadType", jsonString(source.payloadType)},
            {"payloadId", jsonNumber(static_cast<double>(source.payloadId))},
            {"payloadLabel", jsonString(source.payloadLabel)},
            {"startThreshold", jsonNumber(source.startThreshold)},
            {"capturePointer", jsonBool(source.capturePointer)}
        });
    }

    UiSerializedDropTarget parseDropTarget(const UiJsonValue& json)
    {
        UiSerializedDropTarget target;
        if (!json.isObject())
            return target;
        target.enabled = boolOr(json, "enabled", target.enabled);
        target.acceptsAnyPayload = boolOr(json, "acceptsAnyPayload", target.acceptsAnyPayload);
        if (const UiJsonValue* accepted = json.find("acceptedPayloadTypes"))
        {
            if (const Array* array = asArray(*accepted))
            {
                for (const UiJsonValue& value : *array)
                {
                    if (const auto* string = std::get_if<std::string>(&value.value))
                        target.acceptedPayloadTypes.push_back(*string);
                }
            }
        }
        return target;
    }

    UiJsonValue serializeDropTarget(const UiSerializedDropTarget& target)
    {
        Array accepted;
        for (const std::string& type : target.acceptedPayloadTypes)
            accepted.push_back(jsonString(type));
        return jsonObject({
            {"enabled", jsonBool(target.enabled)},
            {"acceptsAnyPayload", jsonBool(target.acceptsAnyPayload)},
            {"acceptedPayloadTypes", jsonArray(std::move(accepted))}
        });
    }

    std::optional<UiStyleTransitionDesc> parseStyleTransition(const UiJsonValue& json)
    {
        if (!json.isObject())
            return std::nullopt;
        UiStyleTransitionDesc transition;
        if (const UiJsonValue* value = json.find("timing"))
            transition.timing = parseTiming(*value);
        transition.animateBackground = boolOr(json, "animateBackground", transition.animateBackground);
        transition.animateForeground = boolOr(json, "animateForeground", transition.animateForeground);
        transition.animateOpacity = boolOr(json, "animateOpacity", transition.animateOpacity);
        return transition;
    }

    UiJsonValue serializeStyleTransition(const UiStyleTransitionDesc& transition)
    {
        return jsonObject({
            {"timing", serializeTiming(transition.timing)},
            {"animateBackground", jsonBool(transition.animateBackground)},
            {"animateForeground", jsonBool(transition.animateForeground)},
            {"animateOpacity", jsonBool(transition.animateOpacity)}
        });
    }

    UiSerializedWidget parseWidget(const UiJsonValue& json)
    {
        UiSerializedWidget widget;
        if (!json.isObject())
            return widget;
        widget.id = stringOr(json, "id", widget.id);
        widget.type = stringOr(json, "type", widget.type);
        widget.text = stringOr(json, "text", widget.text);
        widget.styleClass = stringOr(json, "styleClass", widget.styleClass);
        widget.labelStyleClass = stringOr(json, "labelStyleClass", widget.labelStyleClass);
        widget.imageAsset = stringOr(json, "imageAsset", widget.imageAsset);
        if (const auto value = getString(json, "horizontalAlign"))
            widget.horizontalAlign = parseEnumString(*value, horizontalAlignEntries(), widget.horizontalAlign);
        if (const auto value = getString(json, "verticalAlign"))
            widget.verticalAlign = parseEnumString(*value, verticalAlignEntries(), widget.verticalAlign);
        if (const auto value = getString(json, "wrapMode"))
            widget.wrapMode = parseEnumString(*value, wrapModeEntries(), widget.wrapMode);
        widget.maxLines = intOr(json, "maxLines", widget.maxLines);
        if (const UiJsonValue* value = json.find("imageTint")) widget.imageTint = parseColor(*value, widget.imageTint);
        widget.imageAspect = numberOr(json, "imageAspect", widget.imageAspect);
        widget.rotation = numberOr(json, "rotation", widget.rotation);
        widget.progressValue = numberOr(json, "value", widget.progressValue);
        if (const UiJsonValue* value = json.find("contentOffset")) widget.contentOffset = parseVec2(*value, widget.contentOffset);
        widget.visible = boolOr(json, "visible", widget.visible);
        widget.enabled = boolOr(json, "enabled", widget.enabled);
        widget.interactable = boolOr(json, "interactable", widget.interactable);
        widget.decorative = boolOr(json, "decorative", widget.decorative);
        if (const UiJsonValue* value = json.find("layout")) widget.layout = parseLayout(*value, widget.layout);
        if (const UiJsonValue* value = json.find("semantics"))
        {
            widget.semantics = parseSemantic(*value);
            widget.hasSemantics = true;
        }
        if (const UiJsonValue* value = json.find("navigation")) widget.navigation = parseNavigation(*value);
        if (const UiJsonValue* value = json.find("dragSource")) widget.dragSource = parseDragSource(*value);
        if (const UiJsonValue* value = json.find("dropTarget")) widget.dropTarget = parseDropTarget(*value);
        if (const UiJsonValue* value = json.find("stateTransition")) widget.stateTransition = parseStyleTransition(*value);
        if (const UiJsonValue* value = json.find("bindings"))
        {
            if (const Array* array = asArray(*value))
            {
                for (const UiJsonValue& item : *array)
                    widget.bindings.push_back(parseBinding(item));
            }
        }
        if (const UiJsonValue* value = json.find("children"))
        {
            if (const Array* array = asArray(*value))
            {
                for (const UiJsonValue& item : *array)
                    widget.children.push_back(parseWidget(item));
            }
        }
        return widget;
    }

    UiJsonValue serializeWidget(const UiSerializedWidget& widget)
    {
        Array bindings;
        for (const UiSerializedBinding& binding : widget.bindings)
            bindings.push_back(serializeBinding(binding));
        Array children;
        for (const UiSerializedWidget& child : widget.children)
            children.push_back(serializeWidget(child));

        Object object = {
            {"id", jsonString(widget.id)},
            {"type", jsonString(widget.type)},
            {"layout", serializeLayout(widget.layout)},
            {"styleClass", jsonString(widget.styleClass)},
            {"labelStyleClass", jsonString(widget.labelStyleClass)},
            {"text", jsonString(widget.text)},
            {"imageAsset", jsonString(widget.imageAsset)},
            {"horizontalAlign", jsonString(enumToString(widget.horizontalAlign, horizontalAlignEntries(), "Left"))},
            {"verticalAlign", jsonString(enumToString(widget.verticalAlign, verticalAlignEntries(), "Top"))},
            {"wrapMode", jsonString(enumToString(widget.wrapMode, wrapModeEntries(), "None"))},
            {"maxLines", jsonNumber(widget.maxLines)},
            {"imageTint", serializeColor(widget.imageTint)},
            {"imageAspect", jsonNumber(widget.imageAspect)},
            {"rotation", jsonNumber(widget.rotation)},
            {"value", jsonNumber(widget.progressValue)},
            {"contentOffset", serializeVec2(widget.contentOffset)},
            {"visible", jsonBool(widget.visible)},
            {"enabled", jsonBool(widget.enabled)},
            {"interactable", jsonBool(widget.interactable)},
            {"decorative", jsonBool(widget.decorative)},
            {"navigation", serializeNavigation(widget.navigation)},
            {"bindings", jsonArray(std::move(bindings))},
            {"children", jsonArray(std::move(children))}
        };
        if (widget.hasSemantics)
            object.emplace_back("semantics", serializeSemantic(widget.semantics));
        if (widget.dragSource)
            object.emplace_back("dragSource", serializeDragSource(*widget.dragSource));
        if (widget.dropTarget)
            object.emplace_back("dropTarget", serializeDropTarget(*widget.dropTarget));
        if (widget.stateTransition)
            object.emplace_back("stateTransition", serializeStyleTransition(*widget.stateTransition));
        return jsonObject(std::move(object));
    }

    UiSerializedSurface parseSurface(const UiJsonValue& json)
    {
        UiSerializedSurface surface;
        if (!json.isObject())
            return surface;
        surface.name = stringOr(json, "name", surface.name);
        if (const auto value = getString(json, "kind"))
            surface.kind = parseEnumString(*value, surfaceKindEntries(), surface.kind);
        surface.order = intOr(json, "order", surface.order);
        if (const UiJsonValue* value = json.find("rect"))
            surface.rect = parseRect(*value, surface.rect);
        surface.visible = boolOr(json, "visible", surface.visible);
        surface.enabled = boolOr(json, "enabled", surface.enabled);
        surface.inputEnabled = boolOr(json, "inputEnabled", surface.inputEnabled);
        surface.renderEnabled = boolOr(json, "renderEnabled", surface.renderEnabled);
        return surface;
    }

    UiJsonValue serializeSurface(const UiSerializedSurface& surface)
    {
        return jsonObject({
            {"name", jsonString(surface.name)},
            {"kind", jsonString(enumToString(surface.kind, surfaceKindEntries(), "Screen"))},
            {"order", jsonNumber(surface.order)},
            {"rect", serializeRect(surface.rect)},
            {"visible", jsonBool(surface.visible)},
            {"enabled", jsonBool(surface.enabled)},
            {"inputEnabled", jsonBool(surface.inputEnabled)},
            {"renderEnabled", jsonBool(surface.renderEnabled)}
        });
    }

    UiSerializedLayer parseLayer(const UiJsonValue& json)
    {
        UiSerializedLayer layer;
        if (!json.isObject())
            return layer;
        layer.name = stringOr(json, "name", layer.name);
        layer.order = intOr(json, "order", layer.order);
        layer.state.visible = boolOr(json, "visible", layer.state.visible);
        layer.state.inputEnabled = boolOr(json, "inputEnabled", layer.state.inputEnabled);
        return layer;
    }

    UiJsonValue serializeLayer(const UiSerializedLayer& layer)
    {
        return jsonObject({
            {"name", jsonString(layer.name)},
            {"order", jsonNumber(layer.order)},
            {"visible", jsonBool(layer.state.visible)},
            {"inputEnabled", jsonBool(layer.state.inputEnabled)}
        });
    }

    UiJsonValue serializeAssetJson(const UiSerializedAsset& asset)
    {
        Object object = {
            {"schema", jsonNumber(asset.schemaVersion)},
            {"id", jsonString(asset.id)},
            {"theme", jsonString(asset.theme)},
            {"root", serializeWidget(asset.root)}
        };
        if (asset.surface)
            object.emplace_back("surface", serializeSurface(*asset.surface));

        Array layers;
        for (const UiSerializedLayer& layer : asset.layers)
            layers.push_back(serializeLayer(layer));
        object.emplace_back("layers", jsonArray(std::move(layers)));
        return jsonObject(std::move(object));
    }

    bool isKnownWidgetType(const std::string& type)
    {
        static const std::vector<std::string> known = {
            "Label", "Panel", "Button", "Image", "ProgressBar",
            "Spacer", "Stack", "Separator", "ScrollView"
        };
        return std::find(known.begin(), known.end(), type) != known.end();
    }

    void validateWidget(const UiSerializedWidget& widget,
                        const UiTheme* theme,
                        UiSerializedValidationResult& result,
                        const std::string& path,
                        std::unordered_map<std::string, int>& ids)
    {
        if (widget.type.empty())
            result.error(path, "widget type is required");
        else if (!isKnownWidgetType(widget.type))
            result.error(path, "unknown widget type '" + widget.type + "'");

        if (!widget.id.empty())
            ++ids[widget.id];

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
            validateWidget(widget.children[i], theme, result, path + ".children[" + std::to_string(i) + "]", ids);
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

    struct InstantiationContext
    {
        UiSystem& ui;
        UiSurfaceId surface = UI_DEFAULT_SURFACE;
        UiMountId mount = UI_INVALID_MOUNT;
        const IUiSerializedBindingResolver* bindingResolver = nullptr;
        UiSerializedLoadResult& result;
    };

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

        if (widget.type == "Label")
        {
            gts::ui::UiLabelWidget label;
            gts::ui::UiLabelDesc desc;
            desc.layout = widget.layout;
            desc.text = widget.text;
            desc.styleClass = widget.styleClass.empty() ? desc.styleClass : widget.styleClass;
            desc.visible = widget.visible;
            desc.horizontalAlign = widget.horizontalAlign;
            desc.verticalAlign = widget.verticalAlign;
            desc.wrapMode = widget.wrapMode;
            desc.maxLines = widget.maxLines;
            if (widget.hasSemantics)
            {
                desc.semanticRole = widget.semantics.role;
                desc.accessibilityName = widget.semantics.name;
                desc.accessibilityDescription = widget.semantics.description;
                desc.accessibilityHint = widget.semantics.hint;
                desc.liveRegion = widget.semantics.liveRegion;
                desc.accessibilityHidden = widget.semantics.hidden;
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
            desc.text = widget.text;
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
                desc.semanticRole = widget.semantics.role;
                desc.accessibilityName = widget.semantics.name;
                desc.accessibilityDescription = widget.semantics.description;
                desc.accessibilityHint = widget.semantics.hint;
                desc.liveRegion = widget.semantics.liveRegion;
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
                desc.accessibilityName = widget.semantics.name;
                desc.accessibilityDescription = widget.semantics.description;
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
                desc.accessibilityName = widget.semantics.name;
                desc.accessibilityDescription = widget.semantics.description;
                desc.accessibilityHint = widget.semantics.hint;
                desc.liveRegion = widget.semantics.liveRegion;
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
            context.ui.setSemantics(context.surface, root, widget.semantics);
        applyNavigation(context, root, widget);
        if (widget.type != "Panel" && widget.type != "Button")
            applyDragDrop(context, root, widget);
        applySerializedBindings(context, root, progressFill, widget);

        for (const UiSerializedWidget& child : widget.children)
            instantiateWidget(context, childParent == UI_INVALID_HANDLE ? root : childParent, child);

        return root;
    }
}

const UiJsonValue* UiJsonValue::find(const std::string& key) const
{
    const Object* object = std::get_if<Object>(&value);
    if (object == nullptr)
        return nullptr;

    for (const auto& [memberKey, memberValue] : *object)
    {
        if (memberKey == key)
            return &memberValue;
    }
    return nullptr;
}

bool parseUiJson(std::string_view source, UiJsonValue& outValue, std::string* outError)
{
    JsonParser parser(source);
    return parser.parse(outValue, outError);
}

std::string serializeUiJson(const UiJsonValue& value, int indent)
{
    const std::string prefix = indentString(indent);
    if (std::holds_alternative<std::monostate>(value.value))
        return "null";
    if (const auto* boolean = std::get_if<bool>(&value.value))
        return *boolean ? "true" : "false";
    if (const auto* number = std::get_if<double>(&value.value))
    {
        std::ostringstream out;
        out << *number;
        return out.str();
    }
    if (const auto* string = std::get_if<std::string>(&value.value))
        return "\"" + escapeJson(*string) + "\"";
    if (const auto* array = std::get_if<Array>(&value.value))
    {
        if (array->empty())
            return "[]";
        std::string out = "[\n";
        for (size_t i = 0; i < array->size(); ++i)
        {
            out += indentString(indent + 2) + serializeUiJson((*array)[i], indent + 2);
            out += i + 1 == array->size() ? "\n" : ",\n";
        }
        out += prefix + "]";
        return out;
    }
    if (const auto* object = std::get_if<Object>(&value.value))
    {
        if (object->empty())
            return "{}";
        std::string out = "{\n";
        for (size_t i = 0; i < object->size(); ++i)
        {
            const auto& [key, member] = (*object)[i];
            out += indentString(indent + 2) + "\"" + escapeJson(key) + "\": " +
                   serializeUiJson(member, indent + 2);
            out += i + 1 == object->size() ? "\n" : ",\n";
        }
        out += prefix + "}";
        return out;
    }
    return "null";
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
        validation.error("$", "UI asset root must be an object");
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    UiSerializedAsset asset;
    asset.schemaVersion = intOr(root, "schema", asset.schemaVersion);
    asset.id = stringOr(root, "id", asset.id);
    asset.theme = stringOr(root, "theme", asset.theme);
    if (const UiJsonValue* surface = root.find("surface"))
        asset.surface = parseSurface(*surface);
    if (const UiJsonValue* layers = root.find("layers"))
    {
        if (const Array* array = asArray(*layers))
        {
            for (const UiJsonValue& item : *array)
                asset.layers.push_back(parseLayer(item));
        }
    }
    if (const UiJsonValue* widget = root.find("root"))
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
    return serializeUiJson(serializeAssetJson(asset), 0);
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

bool saveUiSerializedAssetToFile(const std::string& path,
                                 const UiSerializedAsset& asset,
                                 std::string* outError)
{
    std::ofstream file(path);
    if (!file)
    {
        if (outError != nullptr)
            *outError = "failed to open UI asset file for writing";
        return false;
    }
    file << serializeUiSerializedAsset(asset);
    return true;
}

UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset& asset,
                                                       const UiTheme* theme)
{
    UiSerializedValidationResult result;
    if (asset.id.empty())
        result.warning("$.id", "asset id is empty");
    if (asset.schemaVersion != UI_SERIALIZATION_SCHEMA_VERSION)
        result.error("$.schema", "UI asset schema version is not supported");

    std::unordered_map<std::string, int> ids;
    validateWidget(asset.root, theme, result, "$.root", ids);
    for (const auto& [id, count] : ids)
    {
        if (count > 1)
            result.error("$.root", "duplicate widget id '" + id + "'");
    }
    return result;
}

UiSerializedLoadResult UiSerializationRuntime::instantiate(UiSystem& ui,
                                                           UiSurfaceId surface,
                                                           UiMountId mount,
                                                           const UiSerializedAsset& asset,
                                                           const IUiSerializedBindingResolver* bindingResolver,
                                                           const UiTheme* validationTheme)
{
    UiSerializedLoadResult result;
    result.validation = validateUiSerializedAsset(asset, validationTheme);
    if (!result.validation.valid())
        return result;

    UiHandle parent = ui.mountRoot(surface, mount);
    if (parent == UI_INVALID_HANDLE)
    {
        result.validation.error("$.mount", "invalid mount for serialized UI asset");
        return result;
    }

    InstantiationContext context{ui, surface, mount, bindingResolver, result};
    instantiateWidget(context, parent, asset.root);
    result.success = result.validation.valid() && result.instance.root != UI_INVALID_HANDLE;
    return result;
}
