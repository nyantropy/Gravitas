#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "GlmConfig.h"

// answers the question: what type of data do we hold?
enum class GtsVertexSemantic
{
    Position,
    Normal,
    Tangent,
    TexCoord,
    Color,
    Joints,
    Weights
};

// canonical decoded streams
// position/normal: vec3; TexCoord: vec2; Tangent/Color/Weights: vec4; Joints: uvec4
// answers the question: how do we hold that particular type of data?
using GtsVertexAttributeValues = std::variant<
    std::vector<glm::vec3>,
    std::vector<glm::vec2>,
    std::vector<glm::vec4>,
    std::vector<glm::uvec4>>;

// the struct that keeps both of these concepts together - we basically save an array of this
// in our model representation    
struct GtsVertexAttribute
{
    GtsVertexSemantic semantic = GtsVertexSemantic::Position;
    uint32_t setIndex = 0;
    GtsVertexAttributeValues values;
};
