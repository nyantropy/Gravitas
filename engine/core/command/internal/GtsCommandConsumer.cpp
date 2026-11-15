#include "GtsCommandConsumer.h"

#include <iterator>
#include <utility>

#include "GtsCommandStorage.h"

void GtsCommandConsumer::consumeMatchingExtensions(GtsCommandBuffer&                                      buffer,
                                                  const std::function<bool(const GtsExtensionCommand&)>& consume)
{
    auto                    pending = takePendingCommands(buffer);
    std::vector<GtsCommand> remaining;
    remaining.reserve(pending.size());
    for (auto& command : pending)
    {
        const auto* extension = std::get_if<GtsExtensionCommand>(&command);
        if (extension == nullptr || !consume(*extension))
            remaining.push_back(std::move(command));
    }

    auto& queued = buffer.commandStorage().commands;
    queued.insert(queued.begin(), std::make_move_iterator(remaining.begin()), std::make_move_iterator(remaining.end()));
}

std::vector<GtsCommand> GtsCommandConsumer::takePendingCommands(GtsCommandBuffer& buffer)
{
    return std::exchange(buffer.commandStorage().commands, {});
}
