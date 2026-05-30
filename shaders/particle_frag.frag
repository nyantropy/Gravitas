#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 sampled = texture(texSampler, fragTexCoord);
    float distFromCenter = length(fragTexCoord - vec2(0.5));
    float radialAlpha = 1.0 - smoothstep(0.08, 0.5, distFromCenter);

    vec4 color = sampled * fragColor;
    color.a *= radialAlpha;

    if (color.a <= 0.002)
        discard;

    outColor = color;
}
