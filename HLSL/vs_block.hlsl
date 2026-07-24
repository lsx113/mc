#include "light.hlsli"

cbuffer VSConstantBuffer : register(b1)
{
    matrix g_World;
    matrix g_WorldInvTranspose;
}

cbuffer cbPerObject : register(b0)
{
    float4x4 g_ViewProj; // 只有视图和投影矩阵，无世界矩阵
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 Tex : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION; // 在世界中的位置
    float3 NormalW : NORMAL; // 法向量在世界中的方向
    float2 Tex : TEXCOORD;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut vOut;
    float4 posW = mul(float4(vIn.PosL, 1.0f), g_World);

    vOut.PosH = mul(posW, g_ViewProj);
    vOut.PosW = posW.xyz;
    vOut.NormalW = mul(vIn.NormalL, (float3x3) g_WorldInvTranspose);
    vOut.Tex = vIn.Tex; 
    return vOut;
}