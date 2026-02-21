#pragma pack_matrix(column_major)

struct VSInput {
    [[vk::location(0)]] float3 pos : TEXCOORD0;
    [[vk::location(1)]] float3 normal : TEXCOORD1;
};

struct LightingData {
    float4 topAmbient;
    float4 bottomAmbient;
    float4 lightDir;
    float4 fogColor;
    float4 fogParams; // x: start, y: end, z: enabled
    float4 cameraPos;
};

// Set 1 for Vertex Uniforms in SPIR-V
cbuffer Lighting : register(b0, space1) {
    LightingData lighting;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint surfaceIndex : TEXCOORD3;
    float viewDist : TEXCOORD4;
};

struct InstanceData {
    column_major float4x4 mvp;
    column_major float4x4 model;
    float4 color;
    float4 scale;    // Explicit scale passed from CPU
    int surfaces[8]; // 6 indices + 2 padding
};

StructuredBuffer<InstanceData> instanceData : register(t0, space0);

VSOutput main(VSInput input, uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
    VSOutput output;
    InstanceData data = instanceData[instanceID];

    float4 worldPos = mul(data.model, float4(input.pos, 1.0f));
    output.pos = mul(data.mvp, float4(input.pos, 1.0f));

    // Calculate actual distance from camera
    output.viewDist = distance(worldPos.xyz, lighting.cameraPos.xyz);

    // Determine which surface this is (Z+, Z-, X-, X+, Y+, Y-)
    uint faceIdx = vertexID / 6;
    output.surfaceIndex = uint(data.surfaces[faceIdx]);

    // Calculate UVs using explicit scale to prevent stretching on rotated parts.
    float2 uv = float2(0, 0);
    float2 uvScale = float2(1, 1);

    if (faceIdx == 0 || faceIdx == 1) { // Z faces (Front/Back)
        uv = input.pos.xy;
        uvScale = data.scale.xy;
    } else if (faceIdx == 2 || faceIdx == 3) { // X faces (Left/Right)
        uv = float2(input.pos.z, input.pos.y);
        uvScale = data.scale.zy;
    } else { // Y faces (Top/Bottom)
        uv = input.pos.xz;
        uvScale = data.scale.xz;
    }

    // Offset to [0, 1] and apply scale.
    // Multiplying by 0.5f doubles the size of the texture (half as many repetitions).
    output.uv = (uv + 0.5f) * uvScale * 0.5f;

    // Shading
    float3x3 normalMatrix = (float3x3)data.model;
    float3 N = normalize(mul(normalMatrix, input.normal));
    float3 L = normalize(lighting.lightDir.xyz);

    float diffuse = max(0.0, dot(N, L));
    float ambientWeight = N.y * 0.5 + 0.5;
    float3 ambient = lerp(lighting.bottomAmbient.rgb, lighting.topAmbient.rgb, ambientWeight);

    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 finalRGB = data.color.rgb * (ambient + lightColor * diffuse);

    output.color = float4(finalRGB, data.color.a);

    return output;
}
