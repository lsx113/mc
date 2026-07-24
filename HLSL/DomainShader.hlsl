struct DS_OUTPUT
{
	float4 vPosition  : SV_POSITION;
};

cbuffer CBViewProj : register(b0)
{
    matrix gViewProj;
};

struct HS_CONTROL_POINT_OUTPUT
{
    float4 center : TEXCOORD0;
    float4 roller : TEXCOORD1;
};


struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor;
	float InsideTessFactor[2]			: SV_InsideTessFactor;
};

#define NUM_CONTROL_POINTS 1

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	const float pi=3.141592653f;
	float3 center = patch[0].center.xyz;
	float3 roller = patch[0].roller.xyz;
    float r = patch[0].center.w;
    float R = patch[0].roller.w;
	//角度映射
    float u = uv.x * 2.0f * pi;
    float v = uv.y * 2.0f * pi;
	//计算局部坐标
    float3 up = abs(roller.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
	float3 U = normalize(cross( roller,up));
    float3 V = normalize(cross(roller, U));
    float3 bigCirclePos = center + R * (cos(u) * U + sin(u) * V);
    float3 dirToTube = cos(v) * (cos(u) * U + sin(u) * V) + sin(v) * roller;
    float3 finalPos = bigCirclePos + r * dirToTube;
	DS_OUTPUT Output;
	float4 worldPos = float4(finalPos, 1.0f);
	worldPos = mul(worldPos, gViewProj);
	Output.vPosition = worldPos;
	return Output;
}
