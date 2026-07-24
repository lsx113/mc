struct GeoOut
{
    float4 PosH : SV_POSITION;
    float2 BarUV : BARUV;
    float Blood : BLOOD;
};

float4 PS(GeoOut pin) : SV_Target
{
    float2 uv = pin.BarUV;
    float border = 0.1f;
    bool inBorder = uv.x < border || uv.x > 1.0f - border ||
                    uv.y < border || uv.y > 1.0f - border;
    if (inBorder)
        return float4(0, 0, 0, 1); // ºÚÉ«±ß¿ò

    // ±³¾°É«£¨Éî»Ò£©
    float4 bgColor = float4(0.2f, 0.2f, 0.2f, 1.0f);
    // Ç°¾°É«½¥±ä£ººì -> »Æ -> ÂÌ
    float t = pin.Blood;
    float4 fgColor;
    if (t <= 0.5f)
        fgColor = lerp(float4(1, 0, 0, 1), float4(1, 1, 0, 1), t * 2.0f);
    else
        fgColor = lerp(float4(1, 1, 0, 1), float4(0, 1, 0, 1), (t - 0.5f) * 2.0f);

    if (uv.x <= pin.Blood)
        return fgColor;
    else
        return bgColor;
}