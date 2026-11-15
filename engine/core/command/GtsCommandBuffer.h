#pragma once
#include <memory>
#include <string>
#include <any>

#include "GtsSceneTransitionData.h"
class GtsCommandConsumer;

// the command buffer struct that the engine uses to execute custom pre defined commands
class GtsCommandBuffer
{
public:
    GtsCommandBuffer();
    ~GtsCommandBuffer();
    GtsCommandBuffer(GtsCommandBuffer&&) noexcept;
    GtsCommandBuffer& operator=(GtsCommandBuffer&&) noexcept;
    GtsCommandBuffer(const GtsCommandBuffer&) = delete;
    GtsCommandBuffer& operator=(const GtsCommandBuffer&) = delete;

    // pauses all simulation systems, controller systems keep running during the pause
    void requestTogglePause();

    // change from one scene to another or load a scene
    void requestChangeScene(std::string name,
                            std::unique_ptr<GtsSceneTransitionData> data = nullptr);

    void requestExtensionCommand(std::string name, std::any payload);

    // quit/shutdown the engine
    void requestQuit();

private:
    friend class GtsCommandConsumer;
    // should only be used by the engine
    struct Storage;
    std::unique_ptr<Storage> storage;
    Storage& commandStorage();
};
