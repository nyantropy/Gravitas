#include "GtsCamera.hpp"

//default camera is gonna be ideal for tetris ig
GtsCamera::GtsCamera(VkExtent2D& swapChainExtent)
{
    viewMatrix = glm::lookAt(glm::vec3(0.0f, 6.0f, 25.0f),
                             glm::vec3(0.0f, 6.0f, 0.0f),
                             glm::vec3(0.0f, 1.0f, 0.0f));

    float aspectRatio = static_cast<float>(swapChainExtent.width) / swapChainExtent.height;

    float FOV = glm::radians(60.0f);
    float nearClip = 0.1f;
    float farClip = 1000.0f;

    projectionMatrix = glm::perspective(FOV, aspectRatio, nearClip, farClip);

    // Correct for Vulkan's inverted y-axis in NDC
    projectionMatrix[1][1] *= -1;
}



glm::mat4 GtsCamera::getViewMatrix()
{
    return viewMatrix;
}

glm::mat4 GtsCamera::getProjectionMatrix()
{
    return projectionMatrix;
}