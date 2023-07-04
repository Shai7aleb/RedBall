#include <Windows.h>
#include <D3D11.h>
#include <iostream>
#include <vector>
#include "lodepng.h"
#include "Data.h"
#include "Config.h"
#include "TextRenderer.h"

#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"d3d11.lib")

//-----------------Types--------------------------
struct Vertex { float x; float y; float u; float v; };
struct Offset { float x; float y; };
struct ConstBuffer { float xscale; float yscale; float xpos_scale; float ypos_scale; };

Offset* GetPositions(unsigned& count);
std::string GetDebugInfo(const std::string& arg);
//---------------Constants----------------------------
const Vertex Triangle[] = { 
                       {-1.0f,1.0f,  0.0f,0.0f},
                       {1.0f,-1.0f,  1.0f,1.0f},
                       {-1.0,-1.0f,  0.0f,1.0f},
                       {1.0f,1.0f,  1.0f,0.0f}
};
const UINT16 Indexes[] = { 0,3,2,3,1,2 };

const float BackgroundColor[4] = { 0.5f,0.5f,0.5f,1.0f };
//-----------------Persistent State-------------------
static ID3D11Device* pMyDirectDevice;
static ID3D11DeviceContext* pMyDirectContext;
static IDXGISwapChain* pMyDirectSwapChain;
static ID3D11RenderTargetView* MyView;
static ID3D11Buffer* pMyVertexBuffer, *pMyInstanceBuffer,*pMyIndexBuffer, *pMyConstantBuffer;
static ID3D11VertexShader* MyVertexShader;
static ID3D11PixelShader* MyPixelShader;
static ID3D11InputLayout* MyInputLayout;
static ID3D11Texture2D* MyTexture;
static ID3D11ShaderResourceView* MyTextureView;
static ID3D11SamplerState* MySamplerState;
static ID3D11BlendState* pMyBlendState;
static float WindowWidth = INITIALWIDTH;
static float WindowHeight = INITIALHEIGHT;
static unsigned ballcount = 0;
static unsigned buffersize = 4; //I would like to keep this to a power of two
static HANDLE EventID;
TS_TextSystem TextSystem;
static TS_FontID MyFont;
static TS_TextID MyText;
extern HANDLE RenderEvent;
extern HANDLE PhysicsEvent;

void EndRender(DWORD ThreadID) {
    EventID = CreateEventW(NULL,FALSE,FALSE,NULL);
    PostThreadMessageW(ThreadID, WM_QUIT, NULL, NULL);
    WaitForSingleObject(EventID, INFINITE);
    CloseHandle(EventID);
}

static void InitRender (HWND OutputWindow) {
    //-----------Swap Chain and viewport-----------------//
    DXGI_SWAP_CHAIN_DESC scDesc;
    
    scDesc.BufferDesc = {INITIALWIDTH,INITIALHEIGHT,{60u,1u}, DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,DXGI_MODE_SCALING_CENTERED };
    scDesc.SampleDesc = { 1u , 0u };
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2u;
    scDesc.OutputWindow = OutputWindow;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Flags = 0;
    
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG /**/, NULL, NULL, D3D11_SDK_VERSION, &scDesc, &pMyDirectSwapChain, &pMyDirectDevice, NULL, &pMyDirectContext);
    
    ID3D11Texture2D* MyBackBuffer;
    pMyDirectSwapChain->GetBuffer(0/*Back buffer*/, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&MyBackBuffer));
    pMyDirectDevice->CreateRenderTargetView(_Post_notnull_ MyBackBuffer, NULL, &MyView);
    pMyDirectContext->OMSetRenderTargets(1u, &MyView, NULL);
    MyBackBuffer->Release();

    D3D11_VIEWPORT vp;
    vp.Width = INITIALWIDTH;
    vp.Height = INITIALHEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    pMyDirectContext->RSSetViewports(1u, &vp);
    
    //---------------Shaders---------------------
    ID3DBlob* VertexShaderBinary;
    if (D3DReadFileToBlob(L"Ball.vs.cso", &VertexShaderBinary) != S_OK) { MessageBox(NULL, L"Fail 1", L"fail 1", MB_OK); return; } 
    pMyDirectDevice->CreateVertexShader(VertexShaderBinary->GetBufferPointer(), VertexShaderBinary->GetBufferSize(), NULL, &MyVertexShader);
    pMyDirectContext->VSSetShader(MyVertexShader, NULL, NULL);

    ID3DBlob* PixelShaderBinary;
    D3DReadFileToBlob(L"Ball.ps.cso", &PixelShaderBinary);
    pMyDirectDevice->CreatePixelShader(PixelShaderBinary->GetBufferPointer(), PixelShaderBinary->GetBufferSize(), NULL, &MyPixelShader);
    pMyDirectContext->PSSetShader(MyPixelShader, NULL, NULL);
    PixelShaderBinary->Release();

    //----------------Input Layout-------------------------
    D3D11_INPUT_ELEMENT_DESC ipd[] = {
    {"POSITION",0u,DXGI_FORMAT_R32G32_FLOAT,0u,0u,D3D11_INPUT_PER_VERTEX_DATA,0u},
    {"TCOORD",0u,DXGI_FORMAT_R32G32_FLOAT,0u,(unsigned)offsetof(Vertex,u),D3D11_INPUT_PER_VERTEX_DATA,0u},
    {"OFFSET",0u,DXGI_FORMAT_R32G32_FLOAT,1u,0u,D3D11_INPUT_PER_INSTANCE_DATA,1u}
    };
    
    pMyDirectDevice->CreateInputLayout(ipd, (unsigned)std::size(ipd), VertexShaderBinary->GetBufferPointer(), VertexShaderBinary->GetBufferSize(), &MyInputLayout);
    pMyDirectContext->IASetInputLayout(MyInputLayout);
    VertexShaderBinary->Release();

    //------------Vertex, Instance and Index buffers---------------
    //creating Vertex buffer
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = sizeof(Triangle);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.StructureByteStride = 0;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA sbd;
    sbd.pSysMem = Triangle;

    pMyDirectDevice->CreateBuffer(&bd, &sbd, &pMyVertexBuffer); 

    //creating Instance Buffer
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = buffersize * sizeof(Offset);
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    pMyDirectDevice->CreateBuffer(&bd, NULL, &pMyInstanceBuffer);
    //                                                           ^---Create an Empty buffer

    //Binding Vertex and Instance Buffers
    unsigned stride[2] = { sizeof(Vertex),sizeof(Offset) };
    unsigned offset[2] = { 0u,0u };

    pMyDirectContext->IASetVertexBuffers(0u, 1u, &pMyVertexBuffer, &stride[0], &offset[0]);
    pMyDirectContext->IASetVertexBuffers(1u, 1u, &pMyInstanceBuffer, &stride[1], &offset[1]);

    //Setting Primitive Topology
    pMyDirectContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //creating Index Buffer
    bd.ByteWidth = sizeof(Indexes);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.StructureByteStride = sizeof(UINT16);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    sbd.pSysMem = Indexes;

    pMyDirectDevice->CreateBuffer(&bd, &sbd, &pMyIndexBuffer);
    pMyDirectContext->IASetIndexBuffer(pMyIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    //-------------------Setting up the ball texture, view and sampler----------------------
    unsigned char* ImageData;
    unsigned Width, Height;

    lodepng_decode32_file(&ImageData, &Width, &Height, "RedBall.png");

    D3D11_TEXTURE2D_DESC Td = { 0 };
    Td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Td.ArraySize = 1;
    Td.Width = Width;
    Td.Height = Height;
    Td.MipLevels = 1;
    Td.Usage = D3D11_USAGE_IMMUTABLE;
    Td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    Td.SampleDesc = { 1u,0u };
    Td.MiscFlags = 0;
    Td.CPUAccessFlags = 0;

    sbd.pSysMem = ImageData;
    sbd.SysMemPitch = 4 * sizeof(char) * Width;

    pMyDirectDevice->CreateTexture2D(&Td, &sbd, &MyTexture);    

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    srvd.Texture2D.MostDetailedMip = 0;

    pMyDirectDevice->CreateShaderResourceView(MyTexture, &srvd, &MyTextureView);

    pMyDirectContext->PSSetShaderResources(0u, 1u, &MyTextureView);


    D3D11_SAMPLER_DESC sd = {};
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    pMyDirectDevice->CreateSamplerState(&sd, &MySamplerState);

    pMyDirectContext->PSSetSamplers(0u, 1u, &MySamplerState);

    free(ImageData);

    //-----------Enabling Alpha Blending-------------------
    D3D11_BLEND_DESC blenddesc;
    blenddesc.AlphaToCoverageEnable = FALSE;
    blenddesc.IndependentBlendEnable = FALSE;
    blenddesc.RenderTarget[0].BlendEnable = TRUE;
    //note: we assume that the output color is straight (not premultiplied) alpha
    blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; 
    blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blenddesc.RenderTarget[0].RenderTargetWriteMask = 7; //Write to red green and blue only, not alpha


    pMyDirectDevice->CreateBlendState(&blenddesc, &pMyBlendState);
    pMyDirectContext->OMSetBlendState(pMyBlendState, NULL, 0xffffffffu);
    //----------------Creating Constant Buffer--------------
    float scale = BALLDIAMETER/WindowWidth;
    ConstBuffer scales = {1.0f * scale , WindowWidth / WindowHeight * scale , 2.0f/WindowWidth , 2.0f/WindowHeight };
    
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(scales);
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    sbd.pSysMem = &scales;

    pMyDirectDevice->CreateBuffer(&bd, &sbd, &pMyConstantBuffer);

    pMyDirectContext->VSSetConstantBuffers(0u, 1u, &pMyConstantBuffer);
    //---------Text System Init----------
    TextSystem = TSCreateTextSystem(pMyDirectContext);
    //----------------------Synch with the physics thread---------------------------
    SetEvent(RenderEvent);
    WaitForSingleObject(PhysicsEvent, INFINITE);
}

//Init Render Should be called first
static void DrawFrame(void) {
    pMyDirectContext->ClearRenderTargetView(MyView, BackgroundColor);
    
    unsigned count;
    Offset*mem = GetPositions(count);
    ballcount = count;
    if (count > buffersize) {
        do{
            buffersize *= 2;
        } while (count > buffersize);

        pMyInstanceBuffer->Release(); //destroy old buffer

        D3D11_BUFFER_DESC bd;
        bd.ByteWidth = sizeof(Offset) * buffersize;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bd.StructureByteStride = 0;
        bd.MiscFlags = 0;

        unsigned stride = sizeof(Offset);
        unsigned offset = 0;

        pMyDirectDevice->CreateBuffer(&bd, NULL, &pMyInstanceBuffer); //create new buffer with more size
        pMyDirectContext->IASetVertexBuffers(1u, 1u, &pMyInstanceBuffer, &stride, &offset); //rebind to IA
    }
    D3D11_MAPPED_SUBRESOURCE msr;

    pMyDirectContext->Map(pMyInstanceBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &msr);
    memcpy(msr.pData, mem, count * sizeof(Offset));
    pMyDirectContext->Unmap(pMyInstanceBuffer, 0u);
    delete[] mem;

    //Draw red balls
    unsigned stride[] = { sizeof(Vertex),sizeof(Offset) };
    unsigned offset[] = { 0u,0u };
    
    pMyDirectContext->IASetVertexBuffers(0u, 1u, &pMyVertexBuffer, &stride[0], &offset[0]);
    pMyDirectContext->IASetVertexBuffers(1u, 1u, &pMyInstanceBuffer, &stride[1], &offset[1]);
    pMyDirectContext->IASetIndexBuffer(pMyIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    pMyDirectContext->IASetInputLayout(MyInputLayout);
    
    pMyDirectContext->VSSetConstantBuffers(0u, 1u, &pMyConstantBuffer);
    pMyDirectContext->VSSetShader(MyVertexShader, NULL, 0u);
    
    pMyDirectContext->PSSetSamplers(0u, 1u, &MySamplerState);
    pMyDirectContext->PSSetShaderResources(0u, 1u, &MyTextureView);
    pMyDirectContext->PSSetShader(MyPixelShader, NULL, 0u);

    pMyDirectContext->OMSetBlendState(pMyBlendState, NULL, 0xFFFFFFFF);

    pMyDirectContext->DrawIndexedInstanced((UINT)std::size(Indexes), ballcount, 0, 0, 0);
    
    //Draw Text
    TextSystem.TSDrawTexts();
    
    pMyDirectSwapChain->Present(1, 0);
    pMyDirectContext->OMSetRenderTargets(1u, &MyView, NULL);
}

static void Resize(float width, float height) {
    WindowWidth = width;
    WindowHeight = height;
    
    pMyDirectContext->OMSetRenderTargets(0u, 0u, 0u);
    MyView->Release();
    pMyDirectSwapChain->ResizeBuffers(0u, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0u);
    ID3D11Texture2D *Backbuffer;
    pMyDirectSwapChain->GetBuffer(0u, _uuidof(ID3D11Texture2D), (void**)&Backbuffer);
    pMyDirectDevice->CreateRenderTargetView(Backbuffer, NULL, &MyView);
    Backbuffer->Release();
    pMyDirectContext->OMSetRenderTargets(1u, &MyView, 0u);

    D3D11_VIEWPORT vp;
    vp.Width = width;
    vp.Height = height;
    vp.MaxDepth = 1.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    pMyDirectContext->RSSetViewports(1u, &vp);

    float scale = BALLDIAMETER / WindowWidth;
    ConstBuffer scales = { 1.0f * scale , WindowWidth / WindowHeight * scale , 2.0f / WindowWidth , 2.0f / WindowHeight };

    D3D11_MAPPED_SUBRESOURCE msr;
    pMyDirectContext->Map(pMyConstantBuffer,0u, D3D11_MAP_WRITE_DISCARD,0u, &msr);
    memcpy(msr.pData, &scales, sizeof scales);
    pMyDirectContext->Unmap(pMyConstantBuffer, 0u);

    //Updating Text System
    TextSystem.TSUpdateViewport();
}

DWORD RenderThread(LPVOID OutputWindow) {
    InitRender((HWND)OutputWindow);
    MyFont = TextSystem.TSCreateFont("C:\\Windows\\Fonts\\arial.ttf", 24);
    
    char Buffer[256];
    Buffer[0] = '\0';

    TS_Text_Desc TextDesc;
    TextDesc.Content = Buffer;
    TextDesc.Xoffset = 0.0f;
    TextDesc.Xscale = -1.0f;
    TextDesc.Yoffset = 0.0f;
    TextDesc.Yscale = 1.0f;
    TextDesc.Red = 0u;
    TextDesc.Blue = 255u;
    TextDesc.Green = 255u;
    TextDesc.Alpha = 0u;
    TextDesc.ChangeFlags = TS_CONTENT;

    MyText = TextSystem.TSCreateText(MyFont, TextDesc);
    while (1) {
        MSG msg = { 0 };
        bool resize = false;
        LPARAM size = NULL;
        while(PeekMessageW(&msg, NULL, NULL, NULL, PM_REMOVE)) {
            switch (msg.message) {
                case WM_SIZE: {
                    resize = true;
                    size = msg.lParam;
                    break;
                }
                case WM_QUIT: {
                    SetEvent(RenderEvent);
                    WaitForSingleObject(PhysicsEvent,INFINITE);
                    TSDeleteTextSystem(TextSystem);

                    //Destroying All Direct3D resources
                    MySamplerState->Release();
                    MyTexture->Release();
                    MyTextureView->Release();
                    pMyConstantBuffer->Release();
                    pMyBlendState->Release();
                    pMyIndexBuffer->Release();
                    pMyInstanceBuffer->Release();
                    pMyVertexBuffer->Release();
                    MyView->Release();
                    MyPixelShader->Release();
                    MyVertexShader->Release();
                    MyInputLayout->Release();
                    pMyDirectSwapChain->Release();
                    pMyDirectContext->Release();
                    pMyDirectDevice->Release();
                    goto exit;
                    break;
                }
            }
        }
        if (resize) {
            Resize((float)LOWORD(size), (float)HIWORD(size));
        }

        std::string temp = GetDebugInfo("framerate") + "Hz";
        TextDesc.Content = temp.c_str();
        TextSystem.TSUpdateText(MyText, TextDesc);
        DrawFrame();
    }
exit:
    std::cout << "Render thread End\n";
    return 0;
}