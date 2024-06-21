#include "GtsCamera.hpp"

GtsCamera::GtsCamera(VkExtent2D& swapChainExtent)
{
    //std::cout << "Inside!" <<  "Width: " << swapChainExtent.width << "Height: " << swapChainExtent.height << std::endl;
    //std::cout << "radians: " << glm::radians(45.0f) << std::endl;

    //represents the viewmatrix, that transforms world coordinates to camera coordinates
    //first vector: camera positition
    //second vector: where the camera is looking
    viewMatrix = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    //represents the projection matrix with a field of view of 45 degrees, an aspect ratio dependant on the current vulkan viewspace
    //a near clipping plane of 0.1 to keep out precision issues and a far clipping plane of 10.0
    projectionMatrix = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);

    //for whatever reason changes index 1/1 of the projection matrix to a negative number
    //the reason is that the picture looks weird if we dont kekw
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