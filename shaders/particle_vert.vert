#version 450

layout(location = 0) in vec4 inPositionSize;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inRotationDepth;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

vec2 cornerForVertex(int index) {
    vec2 corners[6] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );
    return corners[index];
}

void main() {
    vec2 corner = cornerForVertex(gl_VertexIndex);
    fragTexCoord = corner + vec2(0.5);
    fragColor = inColor;

    float s = sin(inRotationDepth.x);
    float c = cos(inRotationDepth.x);
    vec2 rotated = vec2(
        corner.x * c - corner.y * s,
        corner.x * s + corner.y * c
    ) * inPositionSize.w;

    vec4 viewCenter = cam.view * vec4(inPositionSize.xyz, 1.0);
    vec4 viewPosition = viewCenter + vec4(rotated, 0.0, 0.0);
    gl_Position = cam.proj * viewPosition;
}

