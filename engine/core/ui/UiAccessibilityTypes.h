#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "UiHandle.h"
#include "UiMountTypes.h"
#include "UiTypes.h"

enum class UiSemanticRole : uint8_t
{
    Unknown = 0,
    Window,
    Dialog,
    Panel,
    Button,
    Toggle,
    Checkbox,
    Radio,
    Slider,
    Textbox,
    Password,
    Image,
    Label,
    Heading,
    Group,
    List,
    ListItem,
    Tree,
    TreeItem,
    Table,
    Row,
    Cell,
    ProgressBar,
    Status,
    Toolbar,
    Menu,
    MenuItem,
    Tab,
    TabPanel,
    Viewport,
    Canvas,
    Graph,
    Inspector
};

enum class UiAccessibilityLiveRegion : uint8_t
{
    Off = 0,
    Polite,
    Assertive
};

enum class UiAccessibilityAnnouncementKind : uint8_t
{
    FocusChanged = 0,
    ValueChanged,
    LiveRegion,
    Action,
    StateChanged,
    Custom
};

struct UiSemanticRelationship
{
    std::vector<UiHandle> labelledBy;
    std::vector<UiHandle> describedBy;
    std::vector<UiHandle> controls;
    std::vector<UiHandle> owns;
    UiHandle activeDescendant = UI_INVALID_HANDLE;
    UiHandle popup = UI_INVALID_HANDLE;
    UiHandle tooltip = UI_INVALID_HANDLE;
};

struct UiSemanticDesc
{
    UiSemanticRole role = UiSemanticRole::Unknown;
    std::string name;
    std::string description;
    std::string hint;
    std::string value;
    UiAccessibilityLiveRegion liveRegion = UiAccessibilityLiveRegion::Off;
    UiSemanticRelationship relationships;
    UiMountId ownerMount = UI_INVALID_MOUNT;
    bool hidden = false;
    bool decorative = false;
    bool selected = false;
    bool checked = false;
    bool expanded = false;
    bool readOnly = false;
    bool busy = false;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
    float rangeValue = 0.0f;
    int level = 0;
    int index = 0;
    int count = 0;
};

struct UiSemanticNode
{
    UiHandle handle = UI_INVALID_HANDLE;
    UiHandle parent = UI_INVALID_HANDLE;
    std::vector<UiHandle> children;
    UiSemanticRole role = UiSemanticRole::Unknown;
    std::string name;
    std::string description;
    std::string hint;
    std::string value;
    UiAccessibilityLiveRegion liveRegion = UiAccessibilityLiveRegion::Off;
    UiSemanticRelationship relationships;
    UiMountId ownerMount = UI_INVALID_MOUNT;
    UiRect bounds;
    bool hidden = false;
    bool enabled = true;
    bool focused = false;
    bool selected = false;
    bool checked = false;
    bool expanded = false;
    bool readOnly = false;
    bool busy = false;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
    float rangeValue = 0.0f;
    int level = 0;
    int index = 0;
    int count = 0;
};

struct UiSemanticTree
{
    std::vector<UiSemanticNode> nodes;
};

struct UiAccessibilityAnnouncement
{
    uint64_t sequence = 0;
    UiHandle source = UI_INVALID_HANDLE;
    UiAccessibilityAnnouncementKind kind = UiAccessibilityAnnouncementKind::Custom;
    UiSemanticRole role = UiSemanticRole::Unknown;
    UiAccessibilityLiveRegion liveRegion = UiAccessibilityLiveRegion::Off;
    std::string text;
};

struct UiAccessibilityFrameResult
{
    uint32_t semanticNodes = 0;
    uint32_t announcements = 0;
    uint32_t pruned = 0;
};

struct UiAccessibilityPolicy
{
    bool reducedMotion = false;
    bool highContrast = false;
    float textScale = 1.0f;
};

class IUiAccessibilityAdapter
{
public:
    virtual ~IUiAccessibilityAdapter() = default;
    virtual void publishTree(const UiSemanticTree& tree) = 0;
    virtual void publishAnnouncement(const UiAccessibilityAnnouncement& announcement) = 0;
};
