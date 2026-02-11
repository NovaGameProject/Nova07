struct VSOutput {
    float4 pos : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint surfaceIndex : TEXCOORD3;
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
    
    return float4(finalColor, input.color.a);
}
