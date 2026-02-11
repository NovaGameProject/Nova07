struct VSInput {
    [[vk::location(0)]] float3 pos : TEXCOORD0;
};

// std140 alignment: vec4 is 16 bytes, which matches float4
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
};

// Explicitly use column_major to match GLM
struct InstanceData {
    column_major float4x4 mvp;
    column_major float4x4 model;
    float4 color;
};

// Set 0 for Storage Buffers in SPIR-V
StructuredBuffer<InstanceData> instanceData : register(t0, space0);

VSOutput main(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    
    InstanceData data = instanceData[instanceID];

    // 1. Position (MVP is P * V * M)
    output.pos = mul(data.mvp, float4(input.pos, 1.0f));

    // 2. Normal Calculation (for unit cube -0.5 to 0.5)
    float3 localNormal = float3(0, 0, 0);
    float3 absPos = abs(input.pos);
    if (absPos.x > absPos.y && absPos.x > absPos.z) localNormal.x = sign(input.pos.x);
    else if (absPos.y > absPos.z) localNormal.y = sign(input.pos.y);
    else localNormal.z = sign(input.pos.z);

    // Transform normal to world space using the upper-left 3x3 of the model matrix
    float3x3 normalMatrix = (float3x3)data.model;
    float3 N = normalize(mul(normalMatrix, localNormal));
    float3 L = normalize(lighting.lightDir.xyz);
    
    // 3. Shading (Classic 2007)
    float diffuse = max(0.0, dot(N, L));
    
    // Mix top and bottom ambient based on normal Y in world space
    float ambientWeight = N.y * 0.5 + 0.5;
    float3 ambient = lerp(lighting.bottomAmbient.rgb, lighting.topAmbient.rgb, ambientWeight);
    
    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 finalRGB = data.color.rgb * (ambient + lightColor * diffuse);
    
    output.color = float4(finalRGB, data.color.a);
    
    return output;
}
