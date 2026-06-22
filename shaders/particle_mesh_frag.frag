#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec4 fragInstanceColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 sampled = texture(texSampler, fragTexCoord);
    vec4 color = sampled * vec4(fragColor, 1.0) * fragInstanceColor;
    if (color.a <= 0.002)
        discard;

    outColor = color;
}
