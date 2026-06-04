#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec4 fragTint;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    int vertexColorOnly;
} pc;

void main() {
    if (pc.vertexColorOnly != 0) {
        outColor = vec4(fragColor, 1.0) * fragTint;
        return;
    }

    vec4 color = texture(texSampler, fragTexCoord);
    if (color.a < 0.1)
        discard;
    outColor = color * fragTint;
}
