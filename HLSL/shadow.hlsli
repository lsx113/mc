#ifndef SHADOW_HLSLI
#define SHADOW_HLSLI

// 级联阴影采样：t4 绑定整张纹理数组，s1 是比较采样器
Texture2DArray g_ShadowMap : register(t4);
SamplerComparisonState g_ShadowSampler : register(s1);

cbuffer CBShadow : register(b4)
{
    float4x4 g_lightVP[4];   // 每个级联的光空间视图投影矩阵（已转置）
    float4   g_cascadeSplit; // x~w = 第 1~4 级联的远平面距离
    uint     g_cascadeIndex; // 阴影 pass 当前画第几个级联
    uint3    g_cascadePad;
};

// 按视空间 z 选级联：统计越过几个分割点
int SelectCascade(float viewZ)
{
    int c = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
        c += (viewZ >= g_cascadeSplit[i]) ? 1 : 0;
    return min(c, 3);
}

// 取第 i 个分割距离
float CascadeBoundary(int i)
{
    return i == 0 ? g_cascadeSplit.x
         : i == 1 ? g_cascadeSplit.y
         : i == 2 ? g_cascadeSplit.z
                  : g_cascadeSplit.w;
}

// 单级联采样：一次硬件比较，内部是 2x2 双线性 PCF
float SampleOneCascade(float3 posW, int c)
{
    // 不做任何位置/深度偏移
    float4 sp = mul(float4(posW, 1.0f), g_lightVP[c]);

    // 正交投影，w 恒为 1
    float2 uv = float2(sp.x * 0.5f + 0.5f, 0.5f - sp.y * 0.5f);
    bool outside = (uv.x < 0.0f) || (uv.x > 1.0f) || (uv.y < 0.0f) || (uv.y > 1.0f);

    // 不做深度偏移，直接用接收者的光空间深度参与比较
    float lit = g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler,
                                               float3(uv, (float)c),
                                               sp.z);

    // 落在贴图范围外时按受光处理
    return outside ? 1.0f : lit;
}

// 返回 0（全阴影）~ 1（全亮）
float SampleShadow(float3 posW, float viewZ)
{
    int c = SelectCascade(viewZ);
    float lit = SampleOneCascade(posW, c);

    // 级联边界前后 10% 的过渡带里，和上一级混合，消除硬切
    if (c > 0)
    {
        float boundary = CascadeBoundary(c - 1);
        float band = max(boundary * 0.1f, 0.05f);
        if (viewZ < boundary + band)
        {
            float t = saturate((viewZ - (boundary - band)) / (2.0f * band));
            float litPrev = SampleOneCascade(posW, c - 1);
            lit = lerp(litPrev, lit, t);
        }
    }
    return lit;
}

#endif
