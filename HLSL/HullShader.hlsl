struct VS_OUTPUT
{
    float4 center : TEXCOORD0;
    float4 roller : TEXCOORD1;
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

HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

	Output.EdgeTessFactor[0] = 
		Output.EdgeTessFactor[1] = 
		Output.EdgeTessFactor[2] =
		Output.EdgeTessFactor[3] =
		Output.InsideTessFactor[0] = 30;
		Output.InsideTessFactor[1] = 30;

	return Output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(1)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main( 
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	HS_CONTROL_POINT_OUTPUT Output;

	Output.center = ip[i].center;
	Output.roller = ip[i].roller;

	return Output;
}
