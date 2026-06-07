#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceImage;
layout(push_constant) uniform UpscalePushConstants
{
    vec4 sourceRect;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 sourceUV = pc.sourceRect.xy + fragUV * pc.sourceRect.zw;
    outColor = texture(sourceImage, sourceUV);
}
