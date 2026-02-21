#pragma pack_matrix(column_major)

struct VSOutput {
    float4 pos : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint surfaceIndex : TEXCOORD3;
    float viewDist : TEXCOORD4;
};

struct LightingData {
    float4 topAmbient;
    float4 bottomAmbient;
    float4 lightDir;
    float4 fogColor;
    float4 fogParams; // x: start, y: end, z: enabled
    float4 cameraPos;
};

cbuffer Lighting : register(b0, space1) {
    LightingData lighting;
};

// Texture Array for all surface types
// 0: Smooth, 1: Glue, 2: Weld, 3: Studs, 4: Inlets, 5: Universal, 6: Hinge, 7: Motor, 8: SteppingMotor
Texture2DArray surfaceTextures : register(t0, space2);
SamplerState surfaceSampler : register(s0, space2);

float4 main(VSOutput input) : SV_Target {
    // If surface is Smooth (0), we don't necessarily need to sample,
    // but for simplicity we'll assume the array has a plain white texture at index 0
    float4 texColor = surfaceTextures.Sample(surfaceSampler, float3(input.uv, float(input.surfaceIndex)));

    // In classic Roblox, textures were often multiplied or overlaid.
    // Studs/Inlets were usually dark details on the brick color.
    float3 finalColor = input.color.rgb * texColor.rgb;

    // Fog calculation
    if (lighting.fogParams.z > 0.5) {
        float fogFactor = saturate((input.viewDist - lighting.fogParams.x) / (lighting.fogParams.y - lighting.fogParams.x));
        // Ensure fog doesn't turn everything black if fogColor is wrong,
        // but here we trust the lighting data.
        finalColor = lerp(finalColor, lighting.fogColor.rgb, fogFactor);
    }

    return float4(finalColor, input.color.a);
}
