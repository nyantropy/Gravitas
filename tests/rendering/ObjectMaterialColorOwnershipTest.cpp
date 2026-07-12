#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "MaterialTypes.h"
#include "ObjectUBO.h"
#include "RenderCommand.h"

#ifndef GRAVITAS_ENGINE_ROOT
#define GRAVITAS_ENGINE_ROOT "."
#endif

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    template <typename T, typename = void>
    struct HasTint : std::false_type
    {
    };

    template <typename T>
    struct HasTint<T, std::void_t<decltype(std::declval<T&>().tint)>> : std::true_type
    {
    };

    template <typename T, typename = void>
    struct HasBaseColor : std::false_type
    {
    };

    template <typename T>
    struct HasBaseColor<T, std::void_t<decltype(std::declval<T&>().baseColor)>> : std::true_type
    {
    };

    static_assert(!HasTint<ObjectUBO>::value,
                  "ObjectUBO must not store material tint");
    static_assert(!HasBaseColor<ObjectUBO>::value,
                  "ObjectUBO must not store material base color");
    static_assert(!HasTint<ObjectUploadCommand>::value,
                  "ObjectUploadCommand must not upload material tint");
    static_assert(!HasBaseColor<ObjectUploadCommand>::value,
                  "ObjectUploadCommand must not upload material base color");
    static_assert(alignof(ObjectUBO) == 16,
                  "ObjectUBO must keep Vulkan-friendly 16-byte alignment");
    static_assert(offsetof(ObjectUBO, model) == 0,
                  "ObjectUBO model matrix must be the first field");
    static_assert(offsetof(ObjectUBO, uvTransform) == sizeof(glm::mat4),
                  "ObjectUBO UV transform must follow the model matrix");
    static_assert(sizeof(ObjectUBO) == sizeof(glm::mat4) + sizeof(glm::vec4),
                  "ObjectUBO must contain only model matrix and UV transform");

    std::string readTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
            return {};

        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    bool objectUploadDataStaysObjectOwned()
    {
        ObjectUploadCommand upload;
        upload.objectSSBOSlot = 42;
        upload.modelMatrix[3][0] = 3.0f;
        upload.uvTransform = {2.0f, 2.0f, 0.25f, 0.5f};

        ObjectUBO objectData;
        objectData.model = upload.modelMatrix;
        objectData.uvTransform = upload.uvTransform;

        return require(upload.objectSSBOSlot == 42, "object upload keeps object slot")
            && require(objectData.model[3][0] == 3.0f, "object UBO stores model matrix")
            && require(objectData.uvTransform.x == 2.0f &&
                       objectData.uvTransform.z == 0.25f,
                       "object UBO stores UV transform");
    }

    bool materialFrameStateOwnsBaseColor()
    {
        const glm::vec4 color{0.2f, 0.4f, 0.8f, 0.6f};
        MaterialGpuState state;
        state.instance = {1, 1};
        state.gpuHandle = {2, 1};
        state.parameters = makeMaterialGpuParameters(color, 0.0f, 1.0f);
        state.renderState.alphaMode = MaterialAlphaMode::Blend;
        state.uploadedVersion = 9;

        const MaterialFrameState frameState = makeMaterialFrameState(state);

        return require(frameState.parameters.baseColor == color,
                       "material frame state carries shared base color")
            && require(frameState.renderQueue == RenderQueue::Transparent,
                       "transparent alpha remains material render state");
    }

    bool shadersReadBaseColorFromMaterialState()
    {
        const std::filesystem::path shaderRoot =
            std::filesystem::path(GRAVITAS_ENGINE_ROOT) / "shaders";
        const std::string vertex = readTextFile(shaderRoot / "vertexshader.vert");
        const std::string unlit = readTextFile(shaderRoot / "fragmentshader.frag");
        const std::string pbr = readTextFile(shaderRoot / "fragmentshader_pbr.frag");

        return require(!vertex.empty(), "vertex shader source is readable")
            && require(!unlit.empty(), "unlit fragment shader source is readable")
            && require(!pbr.empty(), "PBR fragment shader source is readable")
            && require(!contains(vertex, "fragTint") &&
                       !contains(unlit, "fragTint") &&
                       !contains(pbr, "fragTint"),
                       "scene shaders do not carry object tint varyings")
            && require(!contains(vertex, "vec4 tint") &&
                       !contains(vertex, "objectData.tint"),
                       "object shader data does not contain tint")
            && require(contains(vertex, "vec4 uvTransform"),
                       "object shader data keeps UV transform")
            && require(contains(unlit, "vec4 baseColor") &&
                       contains(unlit, "pc.baseColor"),
                       "unlit shader consumes material base color")
            && require(contains(pbr, "vec4 baseColor") &&
                       contains(pbr, "pc.baseColor"),
                       "PBR shader consumes material base color");
    }
}

int main()
{
    bool ok = true;
    ok &= objectUploadDataStaysObjectOwned();
    ok &= materialFrameStateOwnsBaseColor();
    ok &= shadersReadBaseColorFromMaterialState();

    if (!ok)
        return 1;

    std::printf("ObjectMaterialColorOwnershipTest passed\n");
    return 0;
}
