Texture2D<float> Texture : register(t0);

SamplerState Sampler : register(s0);

cbuffer TextConst : register(b0) {
	float4 Color; //in premultiplied alpha
	float2 Scale;
	float2 Offset;
};

float4 main(float2 texcoord : TEXCOORD) : SV_TARGET
{
	return Color * Texture.Sample(Sampler,texcoord);
}