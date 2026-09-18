#pragma once

#include <cstdlib>
#include <optional>
#include <string>

class ScopedRuntimeAssetPolicy
{
    public:
    explicit ScopedRuntimeAssetPolicy(const char* value)
    {
        if (const auto* previous = std::getenv("GTS_RUNTIME_ASSET_POLICY"))
            saved = previous;
        set(value);
    }
    ~ScopedRuntimeAssetPolicy()
    {
        set(saved ? saved->c_str() : nullptr);
    }
    static void set(const char* value)
    {
#if defined(_WIN32)
        _putenv_s("GTS_RUNTIME_ASSET_POLICY", value ? value : "");
#else
        if (value)
            setenv("GTS_RUNTIME_ASSET_POLICY", value, 1);
        else
            unsetenv("GTS_RUNTIME_ASSET_POLICY");
#endif
    }

    private:
    std::optional<std::string> saved;
};
