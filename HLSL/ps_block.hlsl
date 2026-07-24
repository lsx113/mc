#include "light.hlsli"

Texture2D g_BackgroundTexture : register(t0);
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

// 雾效参数常量
static const float FogStart = 10.0f; // 雾开始距离
static const float FogEnd = 30.0f; // 雾完全覆盖距离

float4 PS(VertexOut pIn) : SV_Target
{
    float4 Color = g_BackgroundTexture.Sample(g_Sampler, pIn.Tex);
    
    // 标准化法向量
    pIn.NormalW = normalize(pIn.NormalW);
    float3 toEyeW = normalize(g_EyePosW - pIn.PosW);

    // 光照计算
    float4 ambient, diffuse, spec;
    float4 A, D, S;
    ambient = diffuse = spec = A = D = S = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ComputeDirectionalLight(g_Material, g_DirLight, pIn.NormalW, toEyeW, A, D, S);
    ambient += A;
    diffuse += D;
    spec += S;

    float4 litColor = Color * (ambient + diffuse) + spec;
    litColor.a = g_Material.diffuse.a * Color.a;

    // ========== 线性雾效 ==========
    float dist = length(pIn.PosW - g_EyePosW);
    float fogFactor = saturate((dist - FogStart) / (FogEnd - FogStart));
    
    // 根据 daytime 决定雾的颜色：白天淡白，夜晚深蓝
    const float dayLength = 240.0f;
    float angle = (g_Daytime / dayLength) * 2.0f * 3.14159265f;
    float sunHeight = (cos(angle) + 1.0f) * 0.5f; // 0~1
    
    float3 dayFogColor = float3(0.85f, 0.90f, 1.0f); // 淡蓝白
    float3 nightFogColor = float3(0.05f, 0.08f, 0.15f); // 深蓝黑
    
    float3 fogColor = lerp(nightFogColor, dayFogColor, sunHeight);
    
    litColor.rgb = lerp(litColor.rgb, fogColor, fogFactor);
    
    return litColor;
}