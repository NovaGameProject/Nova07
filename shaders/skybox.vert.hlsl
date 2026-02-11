struct VSInput {
    [[vk::location(0)]] float3 position : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 texCoord : TEXCOORD0;
};

#pragma pack_matrix(column_major)

// For SPIR-V Vertex Shaders, Uniform Buffers are in Set 1 (space1)
cbuffer Uniforms : register(b0, space1) {
    float4x4 mvp;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    // xyww trick to ensure skybox is at the far plane
    output.position = output.position.xyww;
    output.texCoord = input.position;
    return output;
}
