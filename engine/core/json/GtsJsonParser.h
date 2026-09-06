#pragma once

#include <string>
#include <string_view>

#include "GtsJsonValue.h"

class GtsJsonParser
{
    public:
    static bool        parse(std::string_view source, GtsJsonValue& output, std::string* error = nullptr);
    static std::string serialize(const GtsJsonValue& value, int indent = 0);
};
