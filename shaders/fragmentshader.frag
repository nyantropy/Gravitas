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
    int lit;
    float specularStrength;
    float shininess;
} pc;

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000000000001)
        return fallback;
    return value * inversesqrt(lengthSquared);
}

void main() {
    if (pc.vertexColorOnly != 0) {
        outColor = vec4(fragColor, 1.0) * fragTint;
        return;
    }

    vec4 color = texture(texSampler, fragTexCoord);
    if (color.a < 0.1)
        discard;

    vec4 base = color * fragTint;
    if (pc.lit == 0) {
        outColor = base;
        return;
    }

    vec3 normal = safeNormalize(fragWorldNormal, vec3(0.0, 0.0, 1.0));
    vec3 directionToLight = safeNormalize(cam.lightDirectionIntensity.xyz, vec3(0.0, 0.0, 1.0));
    vec3 lightColor = max(cam.lightColorAmbient.rgb, vec3(0.0));
    float lightIntensity = max(cam.lightDirectionIntensity.w, 0.0);
    float ambientIntensity = max(cam.lightColorAmbient.w, 0.0);

    vec3 litBase = base.rgb * fragColor;
    vec3 ambient = litBase * ambientIntensity;

    float diffuseFactor = max(dot(normal, directionToLight), 0.0);
    vec3 diffuse = litBase * lightColor * lightIntensity * diffuseFactor;

    vec3 viewDirection = safeNormalize(cam.cameraPosition.xyz - fragWorldPosition, vec3(0.0, 0.0, 1.0));
    vec3 halfDirection = safeNormalize(directionToLight + viewDirection, directionToLight);
    float specularFactor = pow(max(dot(normal, halfDirection), 0.0), max(pc.shininess, 1.0));
    vec3 specular =
        lightColor * lightIntensity * max(pc.specularStrength, 0.0) * specularFactor;

    outColor = vec4(ambient + diffuse + specular, base.a);
}
