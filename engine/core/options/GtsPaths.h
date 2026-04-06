#pragma once

#include <filesystem>

#include "GraphicsConstants.h"

class GtsPaths
{
public:
    static void SetProjectRoot(const std::filesystem::path& root)
    {
        projectRootStorage() = root.lexically_normal();
        projectRootSetStorage() = true;
    }

    static const std::filesystem::path& GetProjectRoot()
    {
        if (projectRootSetStorage())
            return projectRootStorage();

        return fallbackProjectRootStorage();
    }

    static const std::filesystem::path& getProjectRoot()
    {
        return GetProjectRoot();
    }

private:
    static std::filesystem::path& projectRootStorage()
    {
        static std::filesystem::path root;
        return root;
    }

    static bool& projectRootSetStorage()
    {
        static bool isSet = false;
        return isSet;
    }

    static const std::filesystem::path& fallbackProjectRootStorage()
    {
        static const std::filesystem::path fallbackRoot =
            std::filesystem::path(GRAVITAS_ENGINE_ROOT).lexically_normal();
        return fallbackRoot;
    }
};
