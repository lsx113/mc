struct VS_IN
{
    float3 centerW : POSITION; // 对应 ParticleVertex::CenterW
    float sizeW : SIZE; // 对应 ParticleVertex::SizeW
    float4 color : COLOR; // 对应 ParticleVertex::Color
    int type : TYPE; // 对应 ParticleVertex::BlockType
};

struct VS_OUT
{
    float3 center : CENTER;
    float size : SIZE;
    float4 color : COLOR;
    int type : TYPE;
};

VS_OUT VS(VS_IN input)
{
    VS_OUT output;
    output.center = input.centerW; // 直接透传，不再使用世界矩阵
    output.size = input.sizeW;
    output.color = input.color;
    output.type = input.type;
    return output;
}