#pragma once

#include <optional>
#include <string>

std::string keyCodeToString(int keyCode);
std::optional<int> stringToKeyCode(const std::string& name);
