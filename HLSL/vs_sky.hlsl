cbuffer cbPerObject : register(b0)
{
    float4x4 g_ViewProj; // 只有视图和投影矩阵，无世界矩阵
};

struct VSInput
{
    float3 PosL : POSITION;
};

struct VSOutput
{
    float4 PosH : SV_POSITION;
    float3 PosL : TEXCOORD;
};

VSOutput VS(VSInput vin)
{
    VSOutput vout;
    // 移除平移，只保留旋转和缩放（实际上缩放应为1）
    float4 pos = float4(vin.PosL, 1.0f);
    vout.PosH = mul(pos, g_ViewProj).xyww; // 关键技巧：将 w 复制到 z，使深度始终为 1
    vout.PosL = vin.PosL;
    return vout;
}