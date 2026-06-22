#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragSoftness;
layout(location = 3) in vec2 fragLocalCoord;
layout(location = 4) in float fragSpriteShape;
layout(location = 5) in float fragEdgeSoftness;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 0) uniform sampler2D depthSampler;

float shapeAlpha(vec2 uv, float shapeValue, float edgeSoftness) {
    vec2 p = uv - vec2(0.5);
    int shape = int(shapeValue + 0.5);
    float soft = mix(0.004, 0.090, clamp(edgeSoftness, 0.0, 1.0));

    if (shape == 1) {
        float d = max(abs(p.x), abs(p.y));
        return 1.0 - smoothstep(0.5 - soft, 0.5, d);
    }

    if (shape == 2) {
        float d = abs(p.x) + abs(p.y);
        return 1.0 - smoothstep(0.5 - soft, 0.5, d);
    }

    if (shape == 3) {
        vec2 petal = vec2(p.x * 1.75, p.y * 0.82);
        float taper = abs(p.y) * 0.18;
        float d = length(petal) + taper;
        return 1.0 - smoothstep(0.46 - soft, 0.50, d);
    }

    if (shape == 4) {
        float body = 1.0 - smoothstep(0.08, 0.5, abs(p.y));
        float ends = 1.0 - smoothstep(0.36, 0.5, abs(p.x));
        return body * ends;
    }

    float d = length(p);
    return 1.0 - smoothstep(0.08, 0.5, d);
}

void main() {
    vec4 sampled = texture(texSampler, fragTexCoord);
    float maskAlpha = shapeAlpha(fragLocalCoord, fragSpriteShape, fragEdgeSoftness);

    vec4 color = sampled * fragColor;
    color.a *= maskAlpha;

    if (fragSoftness > 0.0) {
        ivec2 depthSize = textureSize(depthSampler, 0);
        vec2 depthUv = gl_FragCoord.xy / vec2(depthSize);
        float sceneDepth = texture(depthSampler, depthUv).r;
        float depthFade = clamp((sceneDepth - gl_FragCoord.z) * fragSoftness, 0.0, 1.0);
        color.a *= depthFade;
    }

    if (color.a <= 0.002)
        discard;

    outColor = color;
}
