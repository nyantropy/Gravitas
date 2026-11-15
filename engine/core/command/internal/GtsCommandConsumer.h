#pragma once

#include <functional>
#include <vector>

#include "GtsCommand.h"

class GtsCommandBuffer;

// Engine consumption only; this directory is not part of the producer target.
class GtsCommandConsumer
{
    public:
    static void consumeMatchingExtensions(GtsCommandBuffer&                                      buffer,
                                         const std::function<bool(const GtsExtensionCommand&)>& consume);
    static std::vector<GtsCommand> takePendingCommands(GtsCommandBuffer& buffer);
};
