
struct Output {
	float2 texcoord : TEXCOORD;
	float4 pos : SV_POSITION;
};

cbuffer TextConst : register(b0) {
	float4 Color;
	float2 Scale;
	float2 Offset;
};

Output main( float4 pos : POSITION, float2 texcoord : TEXCOORD )
{
	Output ret;
	ret.pos = pos;
	ret.texcoord = texcoord;
	return ret;
}