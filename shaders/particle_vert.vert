#version 450

layout(location = 0) in vec4 inPositionSize;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inUvRect;
layout(location = 3) in vec4 inRotationDepth;
layout(location = 4) in vec4 inShapeData;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out float fragSoftness;
layout(location = 3) out vec2 fragLocalCoord;
layout(location = 4) out float fragSpriteShape;
layout(location = 5) out float fragEdgeSoftness;

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

vec2 cornerForVertex(int index) {
    vec2 corners[6] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );
    return corners[index];
}

void main() {
    vec2 corner = cornerForVertex(gl_VertexIndex);
    vec2 normalizedUv = corner + vec2(0.5);
    fragTexCoord = mix(inUvRect.xy, inUvRect.zw, normalizedUv);
    fragLocalCoord = normalizedUv;
    fragColor = inColor;
    fragSoftness = inRotationDepth.z;
    fragSpriteShape = inRotationDepth.w;
    fragEdgeSoftness = inShapeData.x;

    float s = sin(inRotationDepth.x);
    float c = cos(inRotationDepth.x);
    vec2 rotated = vec2(
        corner.x * c - corner.y * s,
        corner.x * s + corner.y * c
    ) * vec2(inPositionSize.w, inRotationDepth.y);

    vec4 viewCenter = cam.view * vec4(inPositionSize.xyz, 1.0);
    vec4 viewPosition = viewCenter + vec4(rotated, 0.0, 0.0);
    gl_Position = cam.proj * viewPosition;
}
