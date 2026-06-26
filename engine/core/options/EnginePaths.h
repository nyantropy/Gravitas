#pragma once

#include <string>

#ifndef GRAVITAS_ENGINE_ROOT
#define GRAVITAS_ENGINE_ROOT ""
#endif

#ifndef GRAVITAS_ENGINE_RESOURCES
#define GRAVITAS_ENGINE_RESOURCES ""
#endif

class EnginePaths
{
public:
    static inline const char* ENGINE_ROOT_CSTR = GRAVITAS_ENGINE_ROOT;
    static inline const char* ENGINE_RESOURCES_CSTR = GRAVITAS_ENGINE_RESOURCES;

    static inline const std::string ENGINE_ROOT = ENGINE_ROOT_CSTR;
    static inline const std::string ENGINE_RESOURCES = ENGINE_RESOURCES_CSTR;
};
