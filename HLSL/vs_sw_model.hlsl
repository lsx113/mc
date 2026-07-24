cbuffer CBcamera : register(b0)
{
    float4x4 g_ViewProj;
}

cbuffer CBPerObject : register(b1)
{
    float4x4 g_World;
    float4x4 g_WorldInvTranspose;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 Tex : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 Tex : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), g_World);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, g_ViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3) g_WorldInvTranspose);
    vout.Tex = vin.Tex;
    return vout;
}