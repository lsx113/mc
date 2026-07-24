Texture2D gTexStone : register(t0);
Texture2D gTexGrass : register(t1);
Texture2D gTexCrystal : register(t2);
SamplerState gSampler : register(s0);

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 tex : TEXCOORD;
    float4 color : COLOR;
    int type : TYPE;
};

float4 PS(PS_IN input) : SV_Target
{
    float4 texColor;
    if (input.type == 1)
        texColor = gTexStone.Sample(gSampler, input.tex);
    else if (input.type == 2)
        texColor = gTexGrass.Sample(gSampler, input.tex);
    else
        texColor = gTexCrystal.Sample(gSampler, input.tex);
    return texColor * input.color;
}