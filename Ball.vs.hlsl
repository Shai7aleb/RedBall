struct VOut {
	float2 texCoord : TCOORD;
	float4 pos : SV_Position;
};

cbuffer Buffer1 : register(b0) {
	float2 scale;
	float2 pos_scale;
};

VOut main(float2 pos : POSITION, float2 texCoord : TCOORD, float2 offset : OFFSET)
{
	VOut Temp;
	Temp.texCoord = texCoord;
	Temp.pos = float4( (pos * scale) + (offset * pos_scale) , 0.0, 1.0);
	return Temp;
}