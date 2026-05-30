#pragma once

#include <string>

namespace gts::tools
{
    struct ToolEntityLabelComponent
    {
        std::string name;
        std::string category;
        bool selectable = true;
    };
}
