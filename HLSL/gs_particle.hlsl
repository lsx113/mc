cbuffer CBViewProj : register(b0)
{
    matrix gViewProj;
};
cbuffer CBEyePos : register(b1)
{
    float3 gEyePosW;
    float pad;
};

struct GS_IN
{
    float3 center : CENTER;
    float size : SIZE;
    float4 color : COLOR;
    int type : TYPE;
};
struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 tex : TEXCOORD;
    float4 color : COLOR;
    int type : TYPE;
};

[maxvertexcount(6)]
void GS(point GS_IN input[1], inout TriangleStream<PS_IN> triStream)
{
    float3 center = input[0].center;
    float size = input[0].size;

    // 1. 计算指向摄像机的方向向量（从粒子指向摄像机）
    float3 toEye = normalize(gEyePosW - center);

    // 2. 构造公告牌的右向量和上向量
    float3 up = float3(0, 1, 0);
    float3 right = normalize(cross(up, toEye));
    float3 newUp = normalize(cross(toEye, right));

    // 3. 计算四个角的偏移量
    float halfSize = size * 0.5f;
    float3 offsets[4] =
    {
        -right * halfSize - newUp * halfSize, // 左下
         right * halfSize - newUp * halfSize, // 右下
         right * halfSize + newUp * halfSize, // 右上
        -right * halfSize + newUp * halfSize // 左上
    };
    float2 texCoords[4] =
    {
        float2(0, 1), float2(1, 1), float2(1, 0), float2(0, 0)
    };

    PS_IN output;
    // 第一个三角形（0-1-2）
    for (int i = 0; i < 3; ++i)
    {
        float3 worldPos = center + offsets[i];
        output.posH = mul(float4(worldPos, 1.0f), gViewProj);
        output.tex = texCoords[i];
        output.color = input[0].color;
        output.type = input[0].type;
        triStream.Append(output);
    }
    triStream.RestartStrip();

    // 第二个三角形（0-2-3）
    int idx2[3] = { 0, 2, 3 };
    for (int j = 0; j < 3; ++j)
    {
        float3 worldPos = center + offsets[idx2[j]];
        output.posH = mul(float4(worldPos, 1.0f), gViewProj);
        output.tex = texCoords[idx2[j]];
        output.color = input[0].color;
        output.type = input[0].type;
        triStream.Append(output);
    }
    triStream.RestartStrip();
}