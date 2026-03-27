#pragma once

// Tags a UITextComponent entity so HudSystem knows which field to update.
struct HudMarkerComponent
{
    enum class Type { Health, Status, Message };
    Type type = Type::Health;
};
