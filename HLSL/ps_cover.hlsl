Texture2D g_BackgroundTexture : register(t0);
SamplerState g_Sampler : register(s0);
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD;
};
float4 PS(VertexOut pin) : SV_Target
{
    return g_BackgroundTexture.Sample(g_Sampler, pin.Tex);
}