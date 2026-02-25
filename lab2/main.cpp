#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib") 

ID3D11Device* g_GraphicsDevice = nullptr;
ID3D11DeviceContext* g_DeviceContext = nullptr;
IDXGISwapChain* g_ExchangeChain = nullptr;
ID3D11RenderTargetView* g_ColorBuffer = nullptr;

ID3D11VertexShader* g_VertexProcessor = nullptr;
ID3D11PixelShader* g_FragmentProcessor = nullptr;
ID3D11InputLayout* g_VertexStructure = nullptr;
ID3D11Buffer* g_GeometryBuffer = nullptr;
ID3D11Buffer* g_IndexPool = nullptr;

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

LRESULT CALLBACK MainWindowHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace helpers {
    template<typename T>
    void DisposeResource(T*& resource) noexcept {
        if (resource) {
            resource->Release();
            resource = nullptr;
        }
    }
}

static const char* g_VertexShaderCode = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}
)";

static const char* g_PixelShaderCode = R"(
struct PS_INPUT {
    float4 pos : SV_Position;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

struct VertexData {
    float coordinates[3];
    uint32_t colorValue;
};

static const VertexData g_VertexPool[] = {
    { { -0.5f, -0.5f, 0.0f }, 0x00FF0000 },
    { {  0.5f, -0.5f, 0.0f }, 0x0000FF00 },
    { {  0.0f,  0.5f, 0.0f }, 0x000000FF }
};

static const uint16_t g_ElementIndices[] = { 0, 2, 1 };

static ID3DBlob* CompileShaderCode(
    const char* sourceCode,
    const char* functionName,
    const char* targetProfile,
    const char* debugLabel) noexcept
{
    UINT compilationFlags = D3DCOMPILE_ENABLE_STRICTNESS;

    ID3DBlob* compiledCode = nullptr;
    ID3DBlob* errorMessages = nullptr;

    HRESULT result = D3DCompile(
        sourceCode,
        strlen(sourceCode),
        debugLabel,
        nullptr,
        nullptr,
        functionName,
        targetProfile,
        compilationFlags,
        0,
        &compiledCode,
        &errorMessages
    );

    if (FAILED(result)) {
        if (errorMessages) {
            const char* errorText = static_cast<const char*>(errorMessages->GetBufferPointer());
            OutputDebugStringA(errorText);
            MessageBoxA(nullptr, errorText, "Shader Compilation Failed", MB_ICONERROR);
            helpers::DisposeResource(errorMessages);
        }
        return nullptr;
    }

    helpers::DisposeResource(errorMessages);
    return compiledCode;
}

static HRESULT InitializeGraphicsSystem(HWND targetWindow) noexcept {
    HRESULT result = S_OK;

    DXGI_SWAP_CHAIN_DESC swapDescription = {};
    swapDescription.BufferCount = 1;
    swapDescription.BufferDesc.Width = SCREEN_WIDTH;
    swapDescription.BufferDesc.Height = SCREEN_HEIGHT;
    swapDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDescription.BufferDesc.RefreshRate.Numerator = 60;
    swapDescription.BufferDesc.RefreshRate.Denominator = 1;
    swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDescription.OutputWindow = targetWindow;
    swapDescription.SampleDesc.Count = 1;
    swapDescription.SampleDesc.Quality = 0;
    swapDescription.Windowed = TRUE;
    swapDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT creationFlags = 0;

    D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL obtainedLevel;

    result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        &requestedLevel,
        1,
        D3D11_SDK_VERSION,
        &swapDescription,
        &g_ExchangeChain,
        &g_GraphicsDevice,
        &obtainedLevel,
        &g_DeviceContext
    );

    if (FAILED(result))
        return result;

    ID3D11Texture2D* backBufferTexture = nullptr;
    result = g_ExchangeChain->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&backBufferTexture));
    if (FAILED(result))
        return result;

    result = g_GraphicsDevice->CreateRenderTargetView(backBufferTexture, nullptr, &g_ColorBuffer);
    helpers::DisposeResource(backBufferTexture);
    if (FAILED(result))
        return result;

    g_DeviceContext->OMSetRenderTargets(1, &g_ColorBuffer, nullptr);

    return S_OK;
}

static HRESULT BuildSceneComponents() noexcept {
    HRESULT result = S_OK;

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.ByteWidth = sizeof(g_VertexPool);
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexInitData = {};
    vertexInitData.pSysMem = g_VertexPool;

    result = g_GraphicsDevice->CreateBuffer(&vertexBufferDesc, &vertexInitData, &g_GeometryBuffer);
    if (FAILED(result)) return result;

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.ByteWidth = sizeof(g_ElementIndices);
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexInitData = {};
    indexInitData.pSysMem = g_ElementIndices;

    result = g_GraphicsDevice->CreateBuffer(&indexBufferDesc, &indexInitData, &g_IndexPool);
    if (FAILED(result)) return result;

    ID3DBlob* vertexShaderBlob = CompileShaderCode(g_VertexShaderCode, "main", "vs_5_0", "vertex_shader.hlsl");
    if (!vertexShaderBlob) return E_FAIL;

    result = g_GraphicsDevice->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr,
        &g_VertexProcessor
    );
    if (FAILED(result)) {
        helpers::DisposeResource(vertexShaderBlob);
        return result;
    }

    ID3DBlob* pixelShaderBlob = CompileShaderCode(g_PixelShaderCode, "main", "ps_5_0", "pixel_shader.hlsl");
    if (!pixelShaderBlob) {
        helpers::DisposeResource(vertexShaderBlob);
        return E_FAIL;
    }

    result = g_GraphicsDevice->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr,
        &g_FragmentProcessor
    );
    if (FAILED(result)) {
        helpers::DisposeResource(vertexShaderBlob);
        helpers::DisposeResource(pixelShaderBlob);
        return result;
    }

    D3D11_INPUT_ELEMENT_DESC layoutDefinition[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          offsetof(VertexData, coordinates), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,   0,
          offsetof(VertexData, colorValue), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = g_GraphicsDevice->CreateInputLayout(
        layoutDefinition,
        _countof(layoutDefinition),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        &g_VertexStructure
    );

    helpers::DisposeResource(vertexShaderBlob);
    helpers::DisposeResource(pixelShaderBlob);

    return result;
}

static void ShutdownGraphicsSystem() noexcept {
    using helpers::DisposeResource;

    DisposeResource(g_VertexStructure);
    DisposeResource(g_FragmentProcessor);
    DisposeResource(g_VertexProcessor);
    DisposeResource(g_IndexPool);
    DisposeResource(g_GeometryBuffer);
    DisposeResource(g_ColorBuffer);
    DisposeResource(g_ExchangeChain);
    DisposeResource(g_DeviceContext);

    DisposeResource(g_GraphicsDevice);
}

static void DrawScene() noexcept {
    const float backgroundColor[4] = { 0.0f, 0.15f, 0.3f, 1.0f };
    g_DeviceContext->ClearRenderTargetView(g_ColorBuffer, backgroundColor);

    D3D11_VIEWPORT viewportSettings = {};
    viewportSettings.TopLeftX = 0.0f;
    viewportSettings.TopLeftY = 0.0f;
    viewportSettings.Width = static_cast<float>(SCREEN_WIDTH);
    viewportSettings.Height = static_cast<float>(SCREEN_HEIGHT);
    viewportSettings.MinDepth = 0.0f;
    viewportSettings.MaxDepth = 1.0f;
    g_DeviceContext->RSSetViewports(1, &viewportSettings);

    D3D11_RECT clippingArea = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    g_DeviceContext->RSSetScissorRects(1, &clippingArea);

    UINT elementSize = sizeof(VertexData);
    UINT startOffset = 0;
    ID3D11Buffer* buffers[] = { g_GeometryBuffer };
    g_DeviceContext->IASetVertexBuffers(0, 1, buffers, &elementSize, &startOffset);
    g_DeviceContext->IASetIndexBuffer(g_IndexPool, DXGI_FORMAT_R16_UINT, 0);
    g_DeviceContext->IASetInputLayout(g_VertexStructure);
    g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_DeviceContext->VSSetShader(g_VertexProcessor, nullptr, 0);
    g_DeviceContext->PSSetShader(g_FragmentProcessor, nullptr, 0);

    g_DeviceContext->DrawIndexed(3, 0, 0);

    g_ExchangeChain->Present(0, 0);
}

LRESULT CALLBACK MainWindowHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(
    _In_ HINSTANCE instanceHandle,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int displayMode)
{
    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowHandler;
    windowClass.hInstance = instanceHandle;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"GraphicsApplication_Window";

    if (!RegisterClassEx(&windowClass)) {
        MessageBox(nullptr, L"Unable to register window class", L"Initialization Error", MB_ICONERROR);
        return -1;
    }

    HWND mainWindow = CreateWindowEx(
        0,
        windowClass.lpszClassName,
        L"Direct3D 11 - Graphics Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr
    );

    if (!mainWindow) {
        MessageBox(nullptr, L"Failed to create application window", L"Initialization Error", MB_ICONERROR);
        return -1;
    }

    ShowWindow(mainWindow, displayMode);
    UpdateWindow(mainWindow);

    if (FAILED(InitializeGraphicsSystem(mainWindow))) {
        MessageBox(nullptr, L"Graphics system initialization failed", L"Fatal Error", MB_ICONERROR);
        ShutdownGraphicsSystem();
        return -1;
    }

    if (FAILED(BuildSceneComponents())) {
        MessageBox(nullptr, L"Unable to build scene components", L"Fatal Error", MB_ICONERROR);
        ShutdownGraphicsSystem();
        return -1;
    }

    MSG messageQueue = {};
    while (messageQueue.message != WM_QUIT) {
        if (PeekMessage(&messageQueue, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&messageQueue);
            DispatchMessage(&messageQueue);
        }
        else {
            DrawScene();
        }
    }

    ShutdownGraphicsSystem();

    return static_cast<int>(messageQueue.wParam);
}