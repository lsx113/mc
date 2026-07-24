struct VSOutput
{
    float4 PosH : SV_POSITION;
    float3 PosL : TEXCOORD;
};

TextureCube g_SkyMap : register(t0);
SamplerState g_Sampler : register(s0);

// 时间常量缓冲区（与 ps_block 共用槽位 3）
cbuffer PSBuffer : register(b3)
{
    float g_Daytime;
    float3 g_Pad;
}

float4 PS(VSOutput pin) : SV_Target
{
    // 1. 采样原始天空盒颜色
    float4 skyColor = g_SkyMap.Sample(g_Sampler, pin.PosL);
    
    // 2. 将 daytime 映射到太阳高度因子 (0~1)
    const float dayLength = 240.0f; // 一天周期（秒）
    float angle = (g_Daytime / dayLength) * 2.0f * 3.14159265f;
    float sunHeight = (cos(angle) + 1.0f) * 0.5f; // 0(午夜) ~ 1(正午)
    
    // 3. 根据太阳高度调整亮度与色调
    //    - 夜晚降低亮度，并混合冷色调
    //    - 白天保持原色或略微增强
    
    // 基础亮度因子（正午最亮，午夜最暗但保留可见度）
    float brightness = lerp(0.25f, 1.2f, sunHeight);
    
    // 夜晚冷色调混合（淡蓝紫色）
    float3 nightTint = float3(0.5f, 0.6f, 1.0f);
    float3 dayColor = skyColor.rgb * brightness;
    float3 finalColor = lerp(dayColor * nightTint, dayColor, sunHeight);
    
    // 4. 可选：增强日出/日落时的暖色调（当太阳接近地平线时）
    float horizonGlow = sin(angle); // 早晚较高
    float3 sunsetTint = float3(1.0f, 0.7f, 0.5f);
    finalColor = lerp(finalColor, finalColor * sunsetTint, horizonGlow * 0.4f);
    
    return float4(finalColor, skyColor.a);
}