#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in mat4 inModel;
layout(location = 7) in vec4 inInstanceColor;
layout(location = 8) in vec4 inUvTransform;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec4 fragInstanceColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    gl_Position = cam.proj * cam.view * inModel * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord * inUvTransform.xy + inUvTransform.zw;
    fragColor = inColor;
    fragInstanceColor = inInstanceColor;
}
