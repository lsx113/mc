
struct VS_INPUT
{
    float4 center : TEXCOORD0;
    float4 roller : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 center : TEXCOORD0;
    float4 roller : TEXCOORD1;
};

VS_OUTPUT main( VS_INPUT input ) 
{
    VS_OUTPUT output;
    output.center = input.center;
    output.roller = input.roller;
    return output;
}