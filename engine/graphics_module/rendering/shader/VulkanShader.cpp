#include "VulkanShader.hpp"

static std::vector<char> readFile(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) 
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VulkanShader::VulkanShader(VulkanShaderConfig& config)
{
    this->config = config;
    const std::vector<char> code = readFile(config.shaderFile);
    createShaderModule(code);
}

VulkanShader::~VulkanShader()
{
    vkDestroyShaderModule(config.vkDevice, shaderModule, nullptr);
}

VkShaderModule& VulkanShader::getShaderModule()
{
    return shaderModule;
}

void VulkanShader::createShaderModule(const std::vector<char>& code) 
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(config.vkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create shader module!");
    }
}