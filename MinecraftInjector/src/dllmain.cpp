//=== File: dllmain.cpp ===
// Defines the entry point and hooks DirectX11 Present to render an ImGui overlay

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "MinHook.lib")

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
PresentFn oPresent = nullptr;

// DirectX objects
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11DeviceContext*    g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Overlay state
bool showMenu = false;
bool showInjected = true;
auto injectTime = std::chrono::steady_clock::now();

// Forward declarations
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
void InitImGui(IDXGISwapChain* pSwapChain);
void RenderOverlay();
BOOL CreateRenderTarget(IDXGISwapChain* pSwapChain);
void CleanupRenderTarget();

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_pd3dDevice) {
        InitImGui(pSwapChain);
    }
    // Input toggle
    if (GetAsyncKeyState(VK_INSERT) & 1) showMenu = !showMenu;
    RenderOverlay();
    return oPresent(pSwapChain, SyncInterval, Flags);
}

void InitImGui(IDXGISwapChain* pSwapChain) {
    // Get device & context
    pSwapChain->GetDevice(__uuidof(g_pd3dDevice), reinterpret_cast<void**>(&g_pd3dDevice));
    g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

    // Create render target
    CreateRenderTarget(pSwapChain);

    // Obtain HWND
    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    HWND hWnd = sd.OutputWindow;

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Initialize ImGui Win32 + DX11
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
}

BOOL CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    ID3D11Texture2D* pBackBuffer;
    if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer))))
        return FALSE;
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    return TRUE;
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void RenderOverlay() {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Show "Injected" for 5 seconds
    auto now = std::chrono::steady_clock::now();
    if (showInjected && std::chrono::duration_cast<std::chrono::seconds>(now - injectTime).count() < 5) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 150, 10), ImGuiCond_Always);
        ImGui::Begin("Injected", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Injected");
        ImGui::End();
    } else showInjected = false;

    // Main toggleable menu
    if (showMenu) {
        ImGui::Begin("Demo Menu", &showMenu);
        ImGui::Text("Hello from injected DLL!");
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

DWORD WINAPI InitializeHook(LPVOID lpReserved) {
    // Wait for D3D11 module
    while (!GetModuleHandleA("d3d11.dll")) Sleep(100);
    // Create dummy device to get VTable
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swap = nullptr;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &swap, &dev, nullptr, &ctx);

    // Hook Present
    void** vTable = *reinterpret_cast<void***>(swap);
    MH_Initialize();
    MH_CreateHook(vTable[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
    MH_EnableHook(vTable[8]);

    // Cleanup dummy
    if (swap) swap->Release();
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        injectTime = std::chrono::steady_clock::now();
        CreateThread(nullptr, 0, InitializeHook, nullptr, 0, nullptr);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupRenderTarget();
        if (MH_EnabledHook(MH_ALL_HOOKS)) MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    return TRUE;
}
