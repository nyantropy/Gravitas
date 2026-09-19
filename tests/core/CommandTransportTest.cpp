#include "GtsCommandBuffer.h"
#include <stdexcept>

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
    require(moved.commands.size() == 5, "Command count and move transport");
    require(std::holds_alternative<GtsTogglePauseCommand>(moved.commands[0]) &&
            std::holds_alternative<GtsQuitCommand>(moved.commands[2]) &&
            std::get<GtsChangeSceneCommand>(moved.commands[3]).name == "next", "Lifecycle ordering");
    const auto& first = std::get<GtsExtensionCommand>(moved.commands[1]);
    require(first.name == "test.request" && std::any_cast<const Payload&>(first.payload).directory == "original",
            "Extension payload is owned and copied");
    require(std::any_cast<int>(&first.payload) == nullptr, "Wrong payload type fails without throwing");
    require(std::any_cast<const Payload&>(std::get<GtsExtensionCommand>(moved.commands[4]).payload).option == 9,
            "Repeated extension identity retains independent FIFO entries");
    moved.commands.clear();
    require(moved.commands.empty(), "Transport clear");
}
