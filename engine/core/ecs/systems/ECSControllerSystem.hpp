#pragma once

#include <string_view>

#include "EcsControllerContext.hpp"

namespace gts::detail
{
    template<typename T>
    constexpr std::string_view typeName()
    {
#if defined(__clang__)
        constexpr std::string_view pretty = __PRETTY_FUNCTION__;
        constexpr std::string_view prefix = "T = ";
        const size_t start = pretty.find(prefix) + prefix.size();
        const size_t end = pretty.find(']', start);
        return pretty.substr(start, end - start);
#elif defined(__GNUC__)
        constexpr std::string_view pretty = __PRETTY_FUNCTION__;
        constexpr std::string_view prefix = "T = ";
        const size_t start = pretty.find(prefix) + prefix.size();
        const size_t end = pretty.find(';', start);
        return pretty.substr(start, end - start);
#elif defined(_MSC_VER)
        constexpr std::string_view pretty = __FUNCSIG__;
        constexpr std::string_view prefix = "typeName<";
        const size_t start = pretty.find(prefix) + prefix.size();
        const size_t end = pretty.find(">(void)", start);
        return pretty.substr(start, end - start);
#else
        return "ECSControllerSystem";
#endif
    }
}

// Per-frame system with access to all frame-dependent dependencies.
// Suitable for input handling, camera, rendering prep, and UI updates.
class ECSControllerSystem
{
    public:
        virtual ~ECSControllerSystem() = default;
        virtual void update(const EcsControllerContext& ctx) = 0;

        std::string_view getName() const
        {
            return debugName;
        }

        void setDebugName(std::string_view name)
        {
            debugName = name;
        }

    private:
        std::string_view debugName = "ECSControllerSystem";
};
