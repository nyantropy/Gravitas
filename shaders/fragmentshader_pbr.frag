#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec4 fragTint;
layout(location = 3) in vec3 fragWorldPosition;
layout(location = 4) in vec3 fragWorldNormal;
layout(location = 5) in vec4 fragWorldTangent;
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

layout(set = 2, binding = 0) uniform sampler2D baseColorSampler;
layout(set = 2, binding = 1) uniform sampler2D metallicRoughnessSampler;
layout(set = 2, binding = 2) uniform sampler2D normalSampler;
layout(set = 2, binding = 3) uniform sampler2D ambientOcclusionSampler;
layout(set = 2, binding = 4) uniform sampler2D emissiveSampler;

layout(set = 3, binding = 0) uniform sampler2D environmentIrradianceSampler;
layout(set = 3, binding = 1) uniform sampler2D environmentSpecularSampler;
layout(set = 3, binding = 2) uniform sampler2D environmentBrdfSampler;

layout(push_constant) uniform PushConstants {
    ivec4 materialFlags;
    vec4 surfaceFactors;
    vec4 emissiveFactorStrength;
} pc;

const float PI = 3.14159265358979323846;
const float EPSILON = 0.00001;
const float MIN_ROUGHNESS = 0.04;
const int MAX_DIRECTIONAL_LIGHTS = 2;
const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 16;
const int FEATURE_BASE_COLOR_TEXTURE = 1 << 0;
const int FEATURE_METALLIC_ROUGHNESS_TEXTURE = 1 << 1;
const int FEATURE_NORMAL_TEXTURE = 1 << 2;
const int FEATURE_AMBIENT_OCCLUSION_TEXTURE = 1 << 3;
const int FEATURE_EMISSIVE_TEXTURE = 1 << 4;
const float MAX_ENVIRONMENT_PREFILTER_MIP = 5.0;

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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness) {
    vec3 upper = max(vec3(1.0 - clamp(roughness, MIN_ROUGHNESS, 1.0)), f0);
    return f0 + (upper - f0) * pow(1.0 - saturate(cosTheta), 5.0);
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

vec3 evaluateDirectLight(
    vec3 baseRgb,
    vec3 normal,
    vec3 viewDirection,
    vec3 directionToLight,
    vec3 radiance,
    float metallic,
    float roughness)
{
    vec3 lightDirection = safeNormalize(directionToLight, vec3(0.0, 0.0, 1.0));
    vec3 halfDirection = safeNormalize(lightDirection + viewDirection, lightDirection);

    float nDotL = saturate(dot(normal, lightDirection));
    float nDotV = saturate(dot(normal, viewDirection));
    float nDotH = saturate(dot(normal, halfDirection));
    float hDotV = saturate(dot(halfDirection, viewDirection));

    vec3 f0 = mix(vec3(0.04), max(baseRgb, vec3(0.0)), metallic);
    vec3 fresnel = fresnelSchlick(hDotV, f0);
    float distribution = distributionGGX(nDotH, roughness);
    float geometry = geometrySmith(nDotV, nDotL, roughness);
    vec3 specular = (distribution * geometry * fresnel) /
        max(4.0 * nDotV * nDotL, EPSILON);

    vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
    vec3 diffuse = kd * baseRgb / PI;
    return (nDotL > 0.0 && nDotV > 0.0)
        ? (diffuse + specular) * max(radiance, vec3(0.0)) * nDotL
        : vec3(0.0);
}

float pointAttenuation(float distanceToLight, float range) {
    float clampedRange = max(range, 0.01);
    if (distanceToLight >= clampedRange)
        return 0.0;

    float distanceSquared = max(distanceToLight * distanceToLight, 0.01);
    float normalizedDistance = saturate(distanceToLight / clampedRange);
    float cutoffBase = 1.0 - pow(normalizedDistance, 4.0);
    float cutoff = saturate(cutoffBase) * saturate(cutoffBase);
    return cutoff / distanceSquared;
}

float spotConeAttenuation(float coneCos, float innerConeCos, float outerConeCos) {
    if (innerConeCos <= outerConeCos + 0.00001)
        return coneCos >= innerConeCos ? 1.0 : 0.0;

    float t = saturate((coneCos - outerConeCos) / (innerConeCos - outerConeCos));
    return t * t * (3.0 - 2.0 * t);
}

bool hasFeature(int feature) {
    return (pc.materialFlags.y & feature) != 0;
}

vec3 rotateEnvironmentDirectionY(vec3 direction, float rotationRadians) {
    vec3 d = safeNormalize(direction, vec3(0.0, 0.0, 1.0));
    float c = cos(rotationRadians);
    float s = sin(rotationRadians);
    return safeNormalize(
        vec3(d.x * c - d.z * s, d.y, d.x * s + d.z * c),
        d);
}

vec2 equirectangularUv(vec3 direction) {
    vec3 d = safeNormalize(direction, vec3(0.0, 0.0, 1.0));
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
    return vec2(u, v);
}

vec3 fallbackTangentForNormal(vec3 normal) {
    if (abs(normal.z) < 0.999)
        return safeNormalize(cross(vec3(0.0, 0.0, 1.0), normal), vec3(1.0, 0.0, 0.0));
    return vec3(1.0, 0.0, 0.0);
}

vec3 decodeTangentNormal(vec3 encodedNormal, float normalScale) {
    vec3 tangentNormal = encodedNormal * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;
    return safeNormalize(tangentNormal, vec3(0.0, 0.0, 1.0));
}

vec3 applyNormalMap(vec3 geometricNormal, vec4 worldTangent) {
    if (!hasFeature(FEATURE_NORMAL_TEXTURE))
        return geometricNormal;

    vec3 normal = safeNormalize(geometricNormal, vec3(0.0, 0.0, 1.0));
    vec3 tangent = safeNormalize(worldTangent.xyz, fallbackTangentForNormal(normal));
    tangent = tangent - normal * dot(tangent, normal);
    tangent = safeNormalize(tangent, fallbackTangentForNormal(normal));
    vec3 bitangent = safeNormalize(cross(normal, tangent) * worldTangent.w, vec3(0.0, 1.0, 0.0));
    vec3 mappedNormal = decodeTangentNormal(texture(normalSampler, fragTexCoord).rgb, pc.surfaceFactors.z);
    return safeNormalize(mat3(tangent, bitangent, normal) * mappedNormal, normal);
}

vec3 evaluateEnvironmentIbl(
    vec3 baseRgb,
    vec3 normal,
    vec3 viewDirection,
    float metallic,
    float roughness,
    float ambientOcclusion)
{
    float enabled = cam.environmentParameters.z >= 0.5 ? 1.0 : 0.0;
    float intensity = max(cam.environmentParameters.x, 0.0) * enabled;
    if (intensity <= 0.0)
        return vec3(0.0);

    float rotation = cam.environmentParameters.y;
    float nDotV = saturate(dot(normal, viewDirection));
    vec3 f0 = mix(vec3(0.04), max(baseRgb, vec3(0.0)), metallic);
    vec3 fresnel = fresnelSchlickRoughness(nDotV, f0, roughness);
    vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);

    vec3 irradianceDirection = rotateEnvironmentDirectionY(normal, rotation);
    vec3 irradiance =
        texture(environmentIrradianceSampler, equirectangularUv(irradianceDirection)).rgb *
        intensity;
    vec3 diffuse = kd * baseRgb * irradiance * ambientOcclusion;

    vec3 reflectionDirection = reflect(-viewDirection, normal);
    vec3 specularDirection = rotateEnvironmentDirectionY(reflectionDirection, rotation);
    float lod = clamp(roughness, MIN_ROUGHNESS, 1.0) * MAX_ENVIRONMENT_PREFILTER_MIP;
    vec3 prefiltered =
        textureLod(environmentSpecularSampler, equirectangularUv(specularDirection), lod).rgb *
        intensity;
    vec2 brdf = texture(environmentBrdfSampler, vec2(nDotV, roughness)).rg;
    vec3 specular = prefiltered * (fresnel * brdf.x + brdf.y);
    return max(diffuse + specular, vec3(0.0));
}

void main() {
    if (pc.materialFlags.x != 0) {
        outColor = vec4(fragColor, 1.0) * fragTint;
        return;
    }

    vec4 textureColor = texture(baseColorSampler, fragTexCoord);
    if (textureColor.a < 0.1)
        discard;

    vec4 base = textureColor * fragTint * vec4(fragColor, 1.0);
    vec4 metallicRoughnessSample = texture(metallicRoughnessSampler, fragTexCoord);
    float metallicSample = hasFeature(FEATURE_METALLIC_ROUGHNESS_TEXTURE)
        ? metallicRoughnessSample.b
        : 1.0;
    float roughnessSample = hasFeature(FEATURE_METALLIC_ROUGHNESS_TEXTURE)
        ? metallicRoughnessSample.g
        : 1.0;
    float metallic = clamp(pc.surfaceFactors.x * metallicSample, 0.0, 1.0);
    float roughness = clamp(pc.surfaceFactors.y * roughnessSample, MIN_ROUGHNESS, 1.0);
    float ambientOcclusionSample = hasFeature(FEATURE_AMBIENT_OCCLUSION_TEXTURE)
        ? texture(ambientOcclusionSampler, fragTexCoord).r
        : (hasFeature(FEATURE_METALLIC_ROUGHNESS_TEXTURE) ? metallicRoughnessSample.r : 1.0);
    float ambientOcclusion = mix(1.0, ambientOcclusionSample, clamp(pc.surfaceFactors.w, 0.0, 1.0));
    vec3 emissiveSample = hasFeature(FEATURE_EMISSIVE_TEXTURE)
        ? texture(emissiveSampler, fragTexCoord).rgb
        : vec3(1.0);
    vec3 emissive = pc.emissiveFactorStrength.rgb *
        max(pc.emissiveFactorStrength.w, 0.0) *
        emissiveSample;

    vec3 normal = applyNormalMap(
        safeNormalize(fragWorldNormal, vec3(0.0, 0.0, 1.0)),
        fragWorldTangent);
    vec3 viewDirection = safeNormalize(cam.cameraPosition.xyz - fragWorldPosition, vec3(0.0, 0.0, 1.0));
    int directionalCount = min(int(cam.lightingCountsAmbient.x + 0.5), MAX_DIRECTIONAL_LIGHTS);
    int pointCount = min(int(cam.lightingCountsAmbient.y + 0.5), MAX_POINT_LIGHTS);
    int spotCount = min(int(cam.lightingCountsAmbient.z + 0.5), MAX_SPOT_LIGHTS);

    vec3 direct = vec3(0.0);
    for (int i = 0; i < directionalCount; ++i) {
        vec3 directionToLight = cam.directionalDirectionIntensity[i].xyz;
        vec3 radiance =
            max(cam.directionalColor[i].rgb, vec3(0.0)) *
            max(cam.directionalDirectionIntensity[i].w, 0.0);
        direct += evaluateDirectLight(
            base.rgb, normal, viewDirection, directionToLight, radiance, metallic, roughness);
    }

    for (int i = 0; i < pointCount; ++i) {
        vec3 toLight = cam.pointPositionRange[i].xyz - fragWorldPosition;
        float distanceToLight = length(toLight);
        float attenuation = pointAttenuation(distanceToLight, cam.pointPositionRange[i].w);
        if (attenuation <= 0.0)
            continue;

        vec3 directionToLight = safeNormalize(toLight, vec3(0.0, 0.0, 1.0));
        vec3 radiance =
            max(cam.pointColorIntensity[i].rgb, vec3(0.0)) *
            max(cam.pointColorIntensity[i].w, 0.0) *
            attenuation;
        direct += evaluateDirectLight(
            base.rgb, normal, viewDirection, directionToLight, radiance, metallic, roughness);
    }

    for (int i = 0; i < spotCount; ++i) {
        vec3 toLight = cam.spotPositionRange[i].xyz - fragWorldPosition;
        float distanceToLight = length(toLight);
        float distanceAttenuation = pointAttenuation(distanceToLight, cam.spotPositionRange[i].w);
        if (distanceAttenuation <= 0.0)
            continue;

        vec3 directionToLight = safeNormalize(toLight, vec3(0.0, 0.0, 1.0));
        vec3 directionFromLightToFragment = -directionToLight;
        vec3 spotDirection =
            safeNormalize(cam.spotDirectionIntensity[i].xyz, vec3(0.0, 0.0, -1.0));
        float coneCos = dot(directionFromLightToFragment, spotDirection);
        float coneAttenuation = spotConeAttenuation(
            coneCos,
            cam.spotColorInnerCone[i].w,
            cam.spotOuterCone[i].x);
        if (coneAttenuation <= 0.0)
            continue;

        vec3 radiance =
            max(cam.spotColorInnerCone[i].rgb, vec3(0.0)) *
            max(cam.spotDirectionIntensity[i].w, 0.0) *
            distanceAttenuation *
            coneAttenuation;
        direct += evaluateDirectLight(
            base.rgb, normal, viewDirection, directionToLight, radiance, metallic, roughness);
    }

    vec3 ibl = evaluateEnvironmentIbl(
        base.rgb,
        normal,
        viewDirection,
        metallic,
        roughness,
        ambientOcclusion);
    outColor = vec4(max(direct + ibl + emissive, vec3(0.0)), base.a);
}
