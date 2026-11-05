#include "shadow.hlsli"

cbuffer cbPerObject : register(b1)
{
    float4x4 g_world;
    float4x4 g_worldInvTranspose; // 阴影 pass 用不到
};

float4 VS(float3 posL : POSITION) : SV_POSITION
{
    float4 posW = mul(float4(posL, 1.0f), g_world);
    return mul(posW, g_lightVP[g_cascadeIndex]);
}
