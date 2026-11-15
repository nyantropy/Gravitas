#pragma once

#include <vector>

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"

struct GtsCommandBuffer::Storage
{
    std::vector<GtsCommand> commands;
};
