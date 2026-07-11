#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in uint instanceObjectIndex;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec4 fragTint;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

struct ObjectData {
    mat4 model;
    vec4 uvTransform;
    vec4 tint;
};

layout(set = 1, binding = 0) readonly buffer ObjectSSBO {
    ObjectData objects[];
};

void main() {
    ObjectData objectData = objects[instanceObjectIndex];
    gl_Position = cam.proj * cam.view * objectData.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord * objectData.uvTransform.xy + objectData.uvTransform.zw;
    fragColor = inColor.rgb;
    fragTint = objectData.tint;
}
