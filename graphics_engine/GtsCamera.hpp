#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>



class GtsCamera
{
    private:
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
    public:
        GtsCamera(VkExtent2D& swapChainExtent);
        glm::mat4 getViewMatrix();
        glm::mat4 getProjectionMatrix();
};