struct VSInput {
    [[vk::location(0)]] float3 pos : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_Position;
};

struct InstanceData {
    float4x4 mvp;
};

// Simplified to space0 for direct slot 0 mapping
StructuredBuffer<InstanceData> instanceData : register(t0, space0);

VSOutput main(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    output.pos = mul(instanceData[instanceID].mvp, float4(input.pos, 1.0f));
    return output;
}
