#pragma once

#include <optional>
#include <string>
#include <vector>

#include "InputBinding.h"

struct InputBindingDocument
{
    int version = 1;
    std::vector<InputBinding> bindings;
};

std::optional<InputBindingDocument> parseInputBindingDocument(const std::string& json);
std::string serializeInputBindingDocument(const std::vector<InputBinding>& bindings, int version = 1);

bool loadInputBindingDocumentFromFile(const std::string& path, InputBindingDocument& document);
bool saveInputBindingDocumentToFile(const std::string& path,
                                    const std::vector<InputBinding>& bindings,
                                    int version = 1);
