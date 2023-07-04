Texture2D Bricks : register(t0);

SamplerState MySampler : register(s0);


float4 main(float2 texCoord : TCOORD) : SV_TARGET
{
	float4 output = Bricks.Sample(MySampler,texCoord);
	return output;
}