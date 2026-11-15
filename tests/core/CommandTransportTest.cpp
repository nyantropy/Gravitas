#include "GtsCommandConsumer.h"
#include "GtsCommandBuffer.h"
#include <stdexcept>

template<class Buffer>
concept ExposesRawCommands = requires(Buffer& buffer) { buffer.commands; };
static_assert(!ExposesRawCommands<GtsCommandBuffer>);

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    struct Payload { std::string directory; int option; };
    GtsCommandBuffer commands;
    commands.requestTogglePause();
    Payload payload{"original", 7};
    commands.requestExtensionCommand("test.request", payload);
    payload.directory = "changed";
    commands.requestQuit();
    commands.requestChangeScene("next");
    commands.requestExtensionCommand("test.request", Payload{"second", 9});
    auto moved = std::move(commands);
    auto pending = GtsCommandConsumer::takePendingCommands(moved);
    require(pending.size() == 5, "Command count and move transport");
    require(std::holds_alternative<GtsTogglePauseCommand>(pending[0]) &&
            std::holds_alternative<GtsQuitCommand>(pending[2]) &&
            std::get<GtsChangeSceneCommand>(pending[3]).name == "next", "Lifecycle ordering");
    const auto& first = std::get<GtsExtensionCommand>(pending[1]);
    require(first.name == "test.request" && std::any_cast<const Payload&>(first.payload).directory == "original",
            "Extension payload is owned and copied");
    require(std::any_cast<int>(&first.payload) == nullptr, "Wrong payload type fails without throwing");
    require(std::any_cast<const Payload&>(std::get<GtsExtensionCommand>(pending[4]).payload).option == 9,
            "Repeated extension identity retains independent FIFO entries");
    pending.clear();
    require(pending.empty() && GtsCommandConsumer::takePendingCommands(moved).empty(), "Transport clear");
    commands.requestQuit();
    require(GtsCommandConsumer::takePendingCommands(commands).size() == 1, "Moved-from producer remains usable");

    std::vector<int> rendered;
    commands.requestTogglePause();
    commands.requestExtensionCommand("render", 1);
    commands.requestChangeScene("first");
    commands.requestExtensionCommand("unknown", 3);
    commands.requestExtensionCommand("render", 2);
    commands.requestQuit();
    GtsCommandConsumer::consumeMatchingExtensions(commands,
        [&](const GtsExtensionCommand& command)
        {
            if (command.name != "render")
                return false;
            rendered.push_back(std::any_cast<int>(command.payload));
            if (rendered.size() == 1)
                commands.requestChangeScene("during-render-dispatch");
            return true;
        });
    require(rendered == std::vector<int>{1, 2}, "Rendering requests retain relative FIFO order");
    pending = GtsCommandConsumer::takePendingCommands(commands);
    require(pending.size() == 5 && std::holds_alternative<GtsTogglePauseCommand>(pending[0])
            && std::get<GtsChangeSceneCommand>(pending[1]).name == "first"
            && std::get<GtsExtensionCommand>(pending[2]).name == "unknown"
            && std::holds_alternative<GtsQuitCommand>(pending[3])
            && std::get<GtsChangeSceneCommand>(pending[4]).name == "during-render-dispatch",
            "Unconsumed and newly enqueued commands retain FIFO order across render drain");
    commands.requestChangeScene("next-batch");
    require(pending.size() == 5 && GtsCommandConsumer::takePendingCommands(commands).size() == 1,
            "Dispatch batch ownership is independent of later producer writes");
}
