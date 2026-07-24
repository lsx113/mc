cbuffer CBcamera : register(b0)
{
    float4x4 g_ViewProj;
}

cbuffer CBswBar : register(b1)
{
    float3 g_EyePosW; // 相机位置
    float g_MaxBlood; // 最大血量（用于归一化）
}

struct VertexOut
{
    float3 CenterW : POSITION;
    float Blood : BLOOD;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float2 BarUV : BARUV;
    float Blood : BLOOD;
};

[maxvertexcount(4)]
void GS(point VertexOut input[1], inout TriangleStream<GeoOut> triStream)
{
    float3 center = input[0].CenterW + float3(0, 2.2f, 0); // 头顶位置
    float3 toEye = normalize(g_EyePosW - center);
    float3 up = float3(0, 1, 0);
    float3 right = normalize(cross(up, toEye));
    float3 newUp = cross(toEye, right);

    float barWidth = 0.8f;
    float barHeight = 0.15f;
    float3 offsets[4] =
    {
        -right * barWidth * 0.5f + newUp * barHeight * 0.5f,
         right * barWidth * 0.5f + newUp * barHeight * 0.5f,
        -right * barWidth * 0.5f - newUp * barHeight * 0.5f,
         right * barWidth * 0.5f - newUp * barHeight * 0.5f
    };
    float2 uvs[4] =
    {
        float2(0, 1), float2(1, 1),
        float2(0, 0), float2(1, 0)
    };

    float bloodPercent = input[0].Blood / g_MaxBlood;

    for (int j = 0; j < 4; ++j)
    {
        GeoOut gout;
        float3 posW = center + offsets[j];
        gout.PosH = mul(float4(posW, 1.0f), g_ViewProj);
        gout.BarUV = uvs[j];
        gout.Blood = bloodPercent;
        triStream.Append(gout);
    }
    triStream.RestartStrip();
}