#pragma once
#include "GtsKeyTranslator.hpp"
#include <GLFW/glfw3.h>

// i wont lie, i dont know how to do it better
class GLFWKeyTranslator : public GtsKeyTranslator
{
    public:
        GtsKey fromPlatformKey(int glfwKey) const override
        {
            switch (glfwKey)
            {
                case GLFW_KEY_W: return GtsKey::W;
                case GLFW_KEY_A: return GtsKey::A;
                case GLFW_KEY_S: return GtsKey::S;
                case GLFW_KEY_D: return GtsKey::D;
                case GLFW_KEY_X: return GtsKey::X;
                case GLFW_KEY_Y: return GtsKey::Y;
                default: return GtsKey::Unknown;
            }
        }

        int toPlatformKey(GtsKey key) const override
        {
            switch (key)
            {
                case GtsKey::W: return GLFW_KEY_W;
                case GtsKey::A: return GLFW_KEY_A;
                case GtsKey::S: return GLFW_KEY_S;
                case GtsKey::D: return GLFW_KEY_D;
                case GtsKey::X: return GLFW_KEY_X;
                case GtsKey::Y: return GLFW_KEY_Y;
                default: return GLFW_KEY_UNKNOWN;
            }
        }
};
