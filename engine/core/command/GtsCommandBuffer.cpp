#include "GtsCommandBuffer.h"

#include <utility>

#include "internal/GtsCommandStorage.h"

GtsCommandBuffer::GtsCommandBuffer()                                       = default;
GtsCommandBuffer::~GtsCommandBuffer()                                      = default;
GtsCommandBuffer::GtsCommandBuffer(GtsCommandBuffer&&) noexcept            = default;
GtsCommandBuffer& GtsCommandBuffer::operator=(GtsCommandBuffer&&) noexcept = default;

GtsCommandBuffer::Storage& GtsCommandBuffer::commandStorage()
{
    if (!storage)
        storage = std::make_unique<Storage>();
    return *storage;
}

void GtsCommandBuffer::requestTogglePause()
{
    commandStorage().commands.emplace_back(GtsTogglePauseCommand{});
}

void GtsCommandBuffer::requestChangeScene(std::string name, std::unique_ptr<GtsSceneTransitionData> data)
{
    commandStorage().commands.emplace_back(GtsChangeSceneCommand{std::move(name), std::move(data)});
}

void GtsCommandBuffer::requestExtensionCommand(std::string name, std::any payload)
{
    commandStorage().commands.emplace_back(GtsExtensionCommand{std::move(name), std::move(payload)});
}

void GtsCommandBuffer::requestQuit()
{
    commandStorage().commands.emplace_back(GtsQuitCommand{});
}
