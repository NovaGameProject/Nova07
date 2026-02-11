struct VSOutput {
    float4 position : SV_Position;
    float3 texCoord : TEXCOORD0;
};

#pragma pack_matrix(column_major)

// For SPIR-V Fragment Shaders, Sampled Textures are in Set 2 (space2)
[[vk::binding(0, 2)]] SamplerState skySampler : register(s0, space2);
[[vk::binding(0, 2)]] TextureCube skyTexture : register(t0, space2);

float4 main(VSOutput input) : SV_Target {
    return skyTexture.Sample(skySampler, input.texCoord);
}
