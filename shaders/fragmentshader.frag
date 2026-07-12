#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPosition;
layout(location = 3) in vec3 fragWorldNormal;
layout(location = 4) in vec4 fragWorldTangent;
layout(location = 0) out vec4 outColor;

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

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    ivec4 materialFlags;
    vec4 baseColor;
    vec4 surfaceFactors;
    vec4 emissiveFactorStrength;
} pc;

void main() {
    if (pc.materialFlags.x != 0) {
        outColor = pc.baseColor * vec4(fragColor, 1.0);
        return;
    }

    vec4 color = texture(texSampler, fragTexCoord);
    if (color.a < 0.1)
        discard;

    vec4 base = pc.baseColor * color * vec4(fragColor, 1.0);
    outColor = base;
}
