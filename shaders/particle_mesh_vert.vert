#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in mat4 inModel;
layout(location = 9) in vec4 inInstanceColor;
layout(location = 10) in vec4 inUvTransform;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec4 fragInstanceColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightingCountsAmbient;
    vec4 directionalDirectionIntensity[2];
    vec4 directionalColor[2];
    vec4 pointPositionRange[32];
    vec4 pointColorIntensity[32];
    vec4 spotPositionRange[16];
    vec4 spotDirectionIntensity[16];
    vec4 spotColorInnerCone[16];
    vec4 spotOuterCone[16];
} cam;

void main() {
    gl_Position = cam.proj * cam.view * inModel * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord * inUvTransform.xy + inUvTransform.zw;
    fragColor = inColor.rgb;
    fragInstanceColor = inInstanceColor;
}
