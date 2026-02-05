#pragma once
#include <vector>
#include <string>

#include "GtsCommand.h"

struct GtsCommandBuffer
{
    void requestPause()
    {
        commands.push_back({GtsCommand::Type::Pause});
    }

    void requestResume()
    {
        commands.push_back({GtsCommand::Type::Resume});
    }

    void requestLoadScene(const std::string& name)
    {
        GtsCommand cmd;
        cmd.type = GtsCommand::Type::LoadScene;
        cmd.stringArg = name;
        commands.push_back(cmd);
    }

    std::vector<GtsCommand> commands;
};