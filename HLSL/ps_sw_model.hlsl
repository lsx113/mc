#include "light.hlsli"

Texture2D g_Tex : register(t0);
SamplerState g_Sampler : register(s0);

cbuffer PSConstantBuffer : register(b2)
{
    DirectionalLight g_DirLight;
    Material g_Material;
    float3 g_EyePosW;
    float g_Pad;
}

cbuffer PSBuffer : register(b3)
{
    float g_Daytime;
    float3 pad;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 Tex : TEXCOORD;
};

static const float FogStart = 10.0f;
static const float FogEnd = 30.0f;

float4 PS(VertexOut pin) : SV_Target
{
    float4 texColor = g_Tex.Sample(g_Sampler, pin.Tex);

    float3 normal = normalize(pin.NormalW);
    float3 toEye = normalize(g_EyePosW - pin.PosW);

    float4 ambient, diffuse, spec;
    ComputeDirectionalLight(g_Material, g_DirLight, normal, toEye, ambient, diffuse, spec);

    float4 litColor = texColor * (ambient + diffuse) + spec;
    litColor.a = g_Material.diffuse.a * texColor.a;

    // 雾效计算（与方块相同）
    float dist = length(pin.PosW - g_EyePosW);
    float fogFactor = saturate((dist - FogStart) / (FogEnd - FogStart));
    
    const float dayLength = 240.0f;
    float angle = (g_Daytime / dayLength) * 2.0f * 3.14159265f;
    float sunHeight = (cos(angle) + 1.0f) * 0.5f;
    
    float3 dayFogColor = float3(0.85f, 0.90f, 1.0f);
    float3 nightFogColor = float3(0.05f, 0.08f, 0.15f);
    float3 fogColor = lerp(nightFogColor, dayFogColor, sunHeight);
    
    litColor.rgb = lerp(litColor.rgb, fogColor, fogFactor);
    
    return litColor;
}