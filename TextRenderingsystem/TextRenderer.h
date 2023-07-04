#pragma once
#define NOMINMAX
#include <d3d11.h>
#include <ft2build.h>
#include <algorithm>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include FT_FREETYPE_H

typedef uint32_t TS_FontID;
typedef uint32_t TS_TextID;

#define TS_CONTENT 1
#define TS_XOFFSET 2
#define TS_XSCALE 4
#define TS_YOFFSET 8
#define TS_YSCALE 16
#define TS_MAXLINELENGTH 32
#define TS_COLOR 64
#define TS_CENTERED 128

struct TS_Text_Desc {
	const char* Content;
	float Xoffset, Xscale;
	float Yoffset, Yscale;
	float MaxLineLength;
	unsigned char Red, Green, Blue, Alpha;
	bool Centered;
	uint32_t ChangeFlags;
};

class TS_TextSystem {
	friend TS_TextSystem TSCreateTextSystem(ID3D11DeviceContext* Context);
	friend void TSDeleteTextSystem(TS_TextSystem& DeletedTextSystem);

	ID3D11Device* Device;
	ID3D11DeviceContext* Context;
	ID3D11SamplerState* Sampler;
	ID3D11VertexShader* VertexShader;
	ID3D11PixelShader* PixelShader;
	ID3D11InputLayout* InputLayout;
	ID3D11BlendState* BlendState;
	ID3D11Buffer* TextConstantBuffer;
	D3D11_VIEWPORT Viewport;

	struct Vertex {
		float Xpos, Ypos;
		float Ucoord, Vcoord;
	};

	struct Text {
		bool inuse;
		float Xoff, Xscl; //positions (offset in pixels)
		float Yoff, Yscl;
		float MaxLineLength; //also in pixels
		unsigned char Red;
		unsigned char Green;
		unsigned char Blue;
		unsigned char Alpha;
		bool Centered;
		std::string TextString;
		std::vector<Vertex> Vertices;
		ID3D11Buffer* VertexBuffer;
		std::vector<unsigned short> Indices;
		ID3D11Buffer* IndexBuffer;
	};

	struct Font {
		bool inuse;
		ID3D11Texture2D* Texture;
		ID3D11ShaderResourceView* TextureView;
		std::vector<Text> Texts;
		unsigned long AtlasWidth;
		unsigned long AtlasHeight;

		struct Character {
			float Width;
			float Height;
			float BearingX;
			float BearingY;
			float Advance;
			float TexCoordLeft;
			float TexCoordRight;
			float TexCoordTop;
			float TexCoordBottom;
		}	CharacterSet[128];

		float Ascender;
		float Desender;
		float MaxLineHeight;
		float Size;
		//Font(const char* FontFile, unsigned int Size, ID3D11DeviceContext* Context);
	};

	struct Command {
		uint8_t type;
		union{
			struct {
				TS_TextID TextID;
				TS_Text_Desc TextDescription;
			} UpdateText;
			struct {
				TS_TextID TextID;
			} DeleteText;
			struct {
				TS_FontID FontID;
			} DeleteFont;
		};
	};
	

	std::vector<Font> Fonts;
	DWORD OwnerThread;
	CRITICAL_SECTION CritSection;
	std::queue<Command> CommandBuffer;
public:
	TS_FontID TSCreateFont(const char* FontFile, float Size);
	TS_TextID TSCreateText(TS_FontID FontID, const TS_Text_Desc& TextDescription);
	void TSUpdateViewport(void);
	void TSUpdateText(TS_TextID TextID, const TS_Text_Desc& TextDescription);
	void TSDrawTexts(void);
	void TSDeleteText(TS_TextID TextID);
	void TSDeleteFont(TS_FontID FontID);
};

TS_TextSystem TSCreateTextSystem(ID3D11DeviceContext* Context);
void TSDeleteTextSystem(TS_TextSystem& DeletedTextSystem);
