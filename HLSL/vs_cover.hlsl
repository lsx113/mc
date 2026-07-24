struct VertexIn
{
    float3 PosL : POSITION;
    float2 Tex : TEXCOORD;
};
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = float4(vin.PosL, 1.0f);  // 直接输出NDC坐标
    vout.Tex = vin.Tex;
    return vout;
}