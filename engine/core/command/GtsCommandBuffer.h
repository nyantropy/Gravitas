#pragma once
#include <memory>
#include <vector>
#include <string>

#include "GtsCommand.h"
#include "GtsSceneTransitionData.h"

struct GtsCommandBuffer
{
    void requestTogglePause()
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::TogglePause;
        commands.push_back(std::move(cmd));
    }

    void requestLoadScene(const std::string& name,
                          std::shared_ptr<GtsSceneTransitionData> data = nullptr)
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::LoadScene;
        cmd.stringArg = name;
        cmd.transitionData = std::move(data);
        commands.push_back(cmd);
    }

    void requestChangeScene(const std::string& name,
                            std::shared_ptr<GtsSceneTransitionData> data = nullptr)
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::ChangeScene;
        cmd.stringArg = name;
        cmd.transitionData = std::move(data);
        commands.push_back(cmd);
    }

    void requestScreenshot()
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::Screenshot;
        commands.push_back(std::move(cmd));
    }

    void requestQuit()
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::Quit;
        commands.push_back(std::move(cmd));
    }

    std::vector<GtsCommand> commands;
};
