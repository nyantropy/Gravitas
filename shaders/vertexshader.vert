#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in uint instanceObjectIndex;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragWorldPosition;
layout(location = 3) out vec3 fragWorldNormal;
layout(location = 4) out vec4 fragWorldTangent;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightingCountsAmbient;
    vec4 environmentParameters;
    vec4 directionalDirectionIntensity[2];
    vec4 directionalColor[2];
    vec4 pointPositionRange[32];
    vec4 pointColorIntensity[32];
    vec4 spotPositionRange[16];
    vec4 spotDirectionIntensity[16];
    vec4 spotColorInnerCone[16];
    vec4 spotOuterCone[16];
} cam;

struct ObjectData {
    mat4 model;
    vec4 uvTransform;
};

layout(set = 1, binding = 0) readonly buffer ObjectSSBO {
    ObjectData objects[];
};

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000000000001)
        return fallback;
    return value * inversesqrt(lengthSquared);
}

vec3 fallbackTangentForNormal(vec3 normal) {
    if (abs(normal.z) < 0.999)
        return safeNormalize(cross(vec3(0.0, 0.0, 1.0), normal), vec3(1.0, 0.0, 0.0));
    return vec3(1.0, 0.0, 0.0);
}

void main() {
    ObjectData objectData = objects[instanceObjectIndex];
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    mat3 modelBasis = mat3(objectData.model);
    float determinantValue = determinant(modelBasis);
    mat3 normalMatrix = abs(determinantValue) > 0.00000001
        ? transpose(inverse(modelBasis))
        : mat3(1.0);

    gl_Position = cam.proj * cam.view * worldPosition;
    fragTexCoord = inTexCoord * objectData.uvTransform.xy + objectData.uvTransform.zw;
    fragColor = inColor.rgb;
    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = safeNormalize(normalMatrix * inNormal, vec3(0.0, 0.0, 1.0));

    vec3 worldTangent = safeNormalize(modelBasis * inTangent.xyz, fallbackTangentForNormal(fragWorldNormal));
    worldTangent = worldTangent - fragWorldNormal * dot(worldTangent, fragWorldNormal);
    worldTangent = safeNormalize(worldTangent, fallbackTangentForNormal(fragWorldNormal));
    float handedness = inTangent.w * (determinantValue < 0.0 ? -1.0 : 1.0);
    fragWorldTangent = vec4(worldTangent, handedness);
}
