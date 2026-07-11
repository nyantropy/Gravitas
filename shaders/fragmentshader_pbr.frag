#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec4 fragTint;
layout(location = 3) in vec3 fragWorldPosition;
layout(location = 4) in vec3 fragWorldNormal;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightDirectionIntensity;
    vec4 lightColorAmbient;
} cam;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    int vertexColorOnly;
    float metallic;
    float roughness;
    float reserved0;
} pc;

const float PI = 3.14159265358979323846;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.04;

float saturate(float value) {
    return clamp(value, 0.0, 1.0);
}

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000000000001)
        return fallback;
    return value * inversesqrt(lengthSquared);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float distributionGGX(float nDotH, float roughness) {
    float r = clamp(roughness, MIN_ROUGHNESS, 1.0);
    float alpha = r * r;
    float alphaSquared = alpha * alpha;
    float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, EPSILON);
}

float geometrySchlickGGX(float nDot, float roughness) {
    float r = clamp(roughness, MIN_ROUGHNESS, 1.0) + 1.0;
    float k = (r * r) / 8.0;
    return nDot / max(nDot * (1.0 - k) + k, EPSILON);
}

float geometrySmith(float nDotV, float nDotL, float roughness) {
    return geometrySchlickGGX(nDotV, roughness) *
        geometrySchlickGGX(nDotL, roughness);
}

void main() {
    if (pc.vertexColorOnly != 0) {
        outColor = vec4(fragColor, 1.0) * fragTint;
        return;
    }

    vec4 textureColor = texture(texSampler, fragTexCoord);
    if (textureColor.a < 0.1)
        discard;

    vec4 base = textureColor * fragTint * vec4(fragColor, 1.0);
    float metallic = clamp(pc.metallic, 0.0, 1.0);
    float roughness = clamp(pc.roughness, MIN_ROUGHNESS, 1.0);

    vec3 normal = safeNormalize(fragWorldNormal, vec3(0.0, 0.0, 1.0));
    vec3 viewDirection = safeNormalize(cam.cameraPosition.xyz - fragWorldPosition, vec3(0.0, 0.0, 1.0));
    vec3 directionToLight = safeNormalize(cam.lightDirectionIntensity.xyz, vec3(0.0, 0.0, 1.0));
    vec3 halfDirection = safeNormalize(directionToLight + viewDirection, directionToLight);

    float nDotL = saturate(dot(normal, directionToLight));
    float nDotV = saturate(dot(normal, viewDirection));
    float nDotH = saturate(dot(normal, halfDirection));
    float hDotV = saturate(dot(halfDirection, viewDirection));

    vec3 lightColor = max(cam.lightColorAmbient.rgb, vec3(0.0));
    float lightIntensity = max(cam.lightDirectionIntensity.w, 0.0);
    float ambientIntensity = max(cam.lightColorAmbient.w, 0.0);
    vec3 radiance = lightColor * lightIntensity;

    vec3 f0 = mix(vec3(0.04), max(base.rgb, vec3(0.0)), metallic);
    vec3 fresnel = fresnelSchlick(hDotV, f0);
    float distribution = distributionGGX(nDotH, roughness);
    float geometry = geometrySmith(nDotV, nDotL, roughness);
    vec3 specular = (distribution * geometry * fresnel) /
        max(4.0 * nDotV * nDotL, EPSILON);

    vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
    vec3 diffuse = kd * base.rgb / PI;
    vec3 direct = (nDotL > 0.0 && nDotV > 0.0)
        ? (diffuse + specular) * radiance * nDotL
        : vec3(0.0);
    vec3 ambient = base.rgb * (1.0 - metallic) * ambientIntensity;

    outColor = vec4(max(ambient + direct, vec3(0.0)), base.a);
}
