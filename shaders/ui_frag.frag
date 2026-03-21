#version 450

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(fontAtlas, fragUV);
    if (texColor.a < 0.01)
        discard;
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
