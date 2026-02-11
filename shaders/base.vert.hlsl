struct VSInput {
    [[vk::location(0)]] float3 pos : TEXCOORD0;
    [[vk::location(1)]] float3 normal : TEXCOORD1;
    [[vk::location(2)]] float2 uv : TEXCOORD2;
};

struct LightingData {
    float4 topAmbient;
    float4 bottomAmbient;
    float4 lightDir;
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
};

struct InstanceData {
    column_major float4x4 mvp;
    column_major float4x4 model;
    float4 color;
    int surfaces[6]; // Z+, Z-, X-, X+, Y+, Y-
    float2 padding;
};

StructuredBuffer<InstanceData> instanceData : register(t0, space0);

VSOutput main(VSInput input, uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
    VSOutput output;
    InstanceData data = instanceData[instanceID];

    float4 worldPos = mul(data.model, float4(input.pos, 1.0f));
    output.pos = mul(data.mvp, float4(input.pos, 1.0f));
    
    // Determine which surface this is (Z+, Z-, X-, X+, Y+, Y-)
    uint faceIdx = vertexID / 6;
    output.surfaceIndex = uint(data.surfaces[faceIdx]);

    // Shading
    float3x3 normalMatrix = (float3x3)data.model;
    float3 N = normalize(mul(normalMatrix, input.normal));
    float3 L = normalize(lighting.lightDir.xyz);
    
    // World-Space UV Projection
    float3 absN = abs(N);
    float2 worldUV = float2(0, 0);
    
    if (absN.y > absN.x && absN.y > absN.z) {
        worldUV = worldPos.xz;
    } else if (absN.x > absN.y && absN.x > absN.z) {
        worldUV = float2(worldPos.z, worldPos.y);
    } else {
        worldUV = float2(worldPos.x, worldPos.y);
    }
    
    // Scale UVs back to 0.5f as requested.
    output.uv = worldUV * 0.5f;

    float diffuse = max(0.0, dot(N, L));
    float ambientWeight = N.y * 0.5 + 0.5;
    float3 ambient = lerp(lighting.bottomAmbient.rgb, lighting.topAmbient.rgb, ambientWeight);
    
    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 finalRGB = data.color.rgb * (ambient + lightColor * diffuse);
    
    output.color = float4(finalRGB, data.color.a);
    
    return output;
}
