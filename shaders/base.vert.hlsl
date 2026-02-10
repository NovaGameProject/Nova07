struct VSInput {
    [[vk::location(0)]] float3 pos : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_Position;
};

// Add the column_major modifier
cbuffer SceneData : register(b0, space1) {
    column_major float4x4 mvp;
};

VSOutput main(VSInput input) {
    VSOutput output;
    // Standard Column-Major multiplication
    output.pos = mul(mvp, float4(input.pos, 1.0f));
    return output;
}
