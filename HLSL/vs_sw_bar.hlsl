struct VertexIn
{
    float3 CenterW : POSITION; // 生物包围盒中心（世界坐标）
    float Blood : BLOOD; // 当前血量值（0~10）
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float Blood : BLOOD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.CenterW = vin.CenterW;
    vout.Blood = vin.Blood;
    return vout;
}