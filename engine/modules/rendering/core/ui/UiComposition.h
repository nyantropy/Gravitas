#pragma once

#include "IResourceProvider.hpp"
#include "UiDocument.h"
#include "UiMountTypes.h"

class UiSystem;

using UiCompositionId = uint32_t;

inline constexpr UiCompositionId UI_INVALID_COMPOSITION = 0;

struct UiCompositionContext
{
    UiSystem& ui;
    UiDocument& document;
    IResourceProvider* resources = nullptr;
    UiMountId mount = UI_INVALID_MOUNT;
    UiHandle root = UI_INVALID_HANDLE;
};

class UiComposition
{
public:
    virtual ~UiComposition() = default;

    virtual void build(UiCompositionContext& context) = 0;
    virtual void update(UiCompositionContext& context) {}
    virtual void destroy(UiCompositionContext& context) {}
};
