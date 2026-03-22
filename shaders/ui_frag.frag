#version 450

layout(set = 0, binding = 0) uniform sampler2D uiTexture;

layout(push_constant) uniform PushConstants {
    mat4  proj;
    float useTexture;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    if (pc.useTexture > 0.5)
        outColor = texture(uiTexture, fragUV) * fragColor;
    else
        outColor = fragColor;
}
