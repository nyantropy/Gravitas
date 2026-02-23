#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

struct ObjectData {
    mat4 model;
};

layout(set = 1, binding = 0) readonly buffer ObjectSSBO {
    ObjectData objects[];
};

layout(push_constant) uniform PushConstants {
    uint objectIndex;
} pc;

void main() {
    gl_Position = cam.proj * cam.view * objects[pc.objectIndex].model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}
