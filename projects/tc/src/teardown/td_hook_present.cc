#include "pch.h"
#include "memory/hooks.h"
#include "teardown/td_hook_present.h"
#include "offsets_generated.h"

using namespace tc;
using namespace td;

// Forward Functions
//------------------------------------------------------------------------
HRESULT __stdcall h_createDXGIFactory1(const IID* const riid, void** ppFactory);
HRESULT __stdcall h_D3D12Present(IDXGISwapChain3* pThis, UINT syncInterval, UINT flags);
LRESULT CALLBACK windowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void setupImGui();
void onImGuiRender();

// Types
//------------------------------------------------------------------------
struct FrameContext {
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12Resource* main_render_target_resource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE main_render_target_descriptor;
};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using tCreateSwapChainForHwnd = std::add_pointer_t<HRESULT __fastcall(
    IDXGIFactory                          *pFactory,
    IUnknown                              *pDevice,
    HWND                                  hWnd,
    const DXGI_SWAP_CHAIN_DESC1           *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGIOutput                           *pRestrictToOutput,
    IDXGISwapChain1                       **ppSwapChain
)>;

using tD3D12Present  = std::add_pointer_t<HRESULT(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT Flags)>;
using tResizeBuffers = std::add_pointer_t<HRESULT(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)>;

// Globals
//------------------------------------------------------------------------
namespace {
    static tCreateSwapChainForHwnd      o_CreateSwapChainForHwnd = nullptr;
    static tD3D12Present                o_Present = nullptr;
    static WNDPROC                      g_WindowProc;

    static ID3D12CommandQueue*          g_d3d12CommandQueue               = nullptr;
    static ID3D12Device*                g_d3d12Device                     = nullptr;
    static ID3D12DescriptorHeap*        g_d3d12DescriptorHeapBackBuffers  = nullptr;
    static ID3D12DescriptorHeap*        g_d3d12DescriptorHeapImGuiRender  = nullptr;
    static ID3D12GraphicsCommandList*   g_d3d12CommandList                = nullptr;
    static UINT                         g_buffersCounts                   = 0;
    static FrameContext*                g_frameContext                    = nullptr;

    static bool g_showMenu = false;
    static int64_t g_prevInitializedFor = -1;
    static bool g_initialized = false;
}

// Hooks
//------------------------------------------------------------------------
HRESULT __fastcall h_CreateSwapChainForHwnd(
    IDXGIFactory                          *pFactory,
    IUnknown                              *pDevice,
    HWND                                  hWnd,
    const DXGI_SWAP_CHAIN_DESC1           *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGIOutput                           *pRestrictToOutput,
    IDXGISwapChain1                       **ppSwapChain)
{
    console::writeln("CreateSwapChainForHwnd Called!");

    g_d3d12CommandQueue = (ID3D12CommandQueue*)pDevice;
    HRESULT result = o_CreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

    void** vTable = *reinterpret_cast<void***>(*ppSwapChain);
    auto pD3D11Present = static_cast<tD3D12Present>(vTable[8]);

    mem::hooks::addHook("d3d12::present", pD3D11Present, &h_D3D12Present, &o_Present);

    return result;
};

HRESULT __stdcall h_createDXGIFactory1(const IID* const riid, void** ppFactory) {
    console::writeln("Creating DXGI Factory");

    tc::funcs::renderer::createDXGIFactory1(riid, ppFactory);

    void** factoryVTable = *reinterpret_cast<void***>(*ppFactory);
    mem::hooks::addHook("factory::CreateSwapChainForHwnd", factoryVTable[15], &h_CreateSwapChainForHwnd, &o_CreateSwapChainForHwnd);

    return tc::funcs::renderer::createDXGIFactory1(riid, ppFactory);
}

// Thanks to https://gist.github.com/SomeCrazyGuy/e733451988dde5558dceb155690d8113 for providing the D3D12 code for using ImGui
HRESULT __stdcall h_D3D12Present(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags) {
    if (!g_initialized) {
        setupImGui();

        HWND windowHandle;

        CreateEvent(nullptr, false, false, nullptr);

        DXGI_SWAP_CHAIN_DESC sdesc;
        swapChain->GetDevice(__uuidof(ID3D12Device), (void**)&g_d3d12Device);
        swapChain->GetDesc(&sdesc);
        swapChain->GetHwnd(&windowHandle);
        sdesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sdesc.OutputWindow = windowHandle;
        sdesc.Windowed = ((GetWindowLongPtr(windowHandle, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

        g_buffersCounts = sdesc.BufferCount;
        g_frameContext = (FrameContext*) calloc(g_buffersCounts, sizeof(FrameContext));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
        descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorImGuiRender.NumDescriptors = g_buffersCounts;
        descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        g_d3d12Device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&g_d3d12DescriptorHeapImGuiRender));

        ID3D12CommandAllocator* allocator;
        g_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        for (size_t i = 0; i < g_buffersCounts; i++) {
            g_frameContext[i].commandAllocator = allocator;
        }

        g_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&g_d3d12CommandList));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers;
        descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descriptorBackBuffers.NumDescriptors = g_buffersCounts;
        descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorBackBuffers.NodeMask = 1;

        g_d3d12Device->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&g_d3d12DescriptorHeapBackBuffers));

        const auto rtvDescriptorSize = g_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_d3d12DescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < g_buffersCounts; i++) {
            ID3D12Resource* pBackBuffer = nullptr;
            g_frameContext[i].main_render_target_descriptor = rtvHandle;
            swapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            g_d3d12Device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
            g_frameContext[i].main_render_target_resource = pBackBuffer;
            rtvHandle.ptr += rtvDescriptorSize;
        }

        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplDX12_Init(g_d3d12Device, g_buffersCounts,
            DXGI_FORMAT_R8G8B8A8_UNORM, g_d3d12DescriptorHeapImGuiRender,
            g_d3d12DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
            g_d3d12DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart()
        );

        ImGui_ImplDX12_CreateDeviceObjects();
        g_WindowProc = (WNDPROC)SetWindowLongPtrW(windowHandle, GWLP_WNDPROC, (LONG_PTR)windowProcHook);

        g_initialized = true;
    }

    if (g_showMenu && g_d3d12CommandQueue) {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();

        onImGuiRender();

        FrameContext& currentFrameContext = g_frameContext[swapChain->GetCurrentBackBufferIndex()];
        currentFrameContext.commandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = currentFrameContext.main_render_target_resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

                
        g_d3d12CommandList->Reset(currentFrameContext.commandAllocator, nullptr);
        g_d3d12CommandList->ResourceBarrier(1, &barrier);
        g_d3d12CommandList->OMSetRenderTargets(1, &currentFrameContext.main_render_target_descriptor, FALSE, nullptr);
        g_d3d12CommandList->SetDescriptorHeaps(1, &g_d3d12DescriptorHeapImGuiRender);

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_d3d12CommandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        g_d3d12CommandList->ResourceBarrier(1, &barrier);
        g_d3d12CommandList->Close();

        g_d3d12CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&g_d3d12CommandList));
    }

    return o_Present(swapChain, syncInterval, flags);
}

using t_wglSwapBuffers = std::add_pointer_t<BOOL(HDC hdc)>;
t_wglSwapBuffers o_wglSwapBuffers = nullptr;

BOOL h_wglSwapBuffers(HDC hdc) {
    if (!g_initialized) {
        HWND windowHandle = WindowFromDC(hdc);
        g_WindowProc = (WNDPROC)SetWindowLongPtrW(windowHandle, GWLP_WNDPROC, (LONG_PTR)windowProcHook);

        setupImGui();

        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplOpenGL3_Init();

        g_initialized = true;
    }

    if (g_showMenu) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();

        onImGuiRender();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    return o_wglSwapBuffers(hdc);
}

LRESULT CALLBACK windowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_F8)
        g_showMenu = !g_showMenu;

    if (g_showMenu) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        return true;
    }

    return CallWindowProc(g_WindowProc, hWnd, uMsg, wParam, lParam);
}

void setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = true;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
}

void onImGuiRender() {
    ImGui::NewFrame();

    td::renderer::onRender();

    ImGui::EndFrame();
}

DWORD* __fastcall h_rendererInitFor(DWORD* a1, int* a2) {
    console::writeln("h_rendererInitFor called!");
    int64_t rcx = ((int64_t)*(uint32_t*)a2);

    if (g_prevInitializedFor != -1) {
        g_initialized = false;

        if (g_prevInitializedFor == 0) { // OpenGL
            ImGui_ImplOpenGL3_Shutdown();
        } else if (g_prevInitializedFor == 1) { // DirectX12
            ImGui_ImplDX12_Shutdown();
        }

        g_d3d12Device = nullptr;
        g_d3d12DescriptorHeapBackBuffers = nullptr;
        g_d3d12DescriptorHeapImGuiRender = nullptr;
        g_d3d12CommandList = nullptr;
        g_buffersCounts = 0;
        g_frameContext = nullptr;

        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (rcx == 0) {
        console::writeln("Hooking present for OpenGL");
        HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
        if (opengl32) {
            void* wglSwapBuffersAddr = GetProcAddress(opengl32, "wglSwapBuffers");
            if (wglSwapBuffersAddr) {
                mem::hooks::addHook("wglSwapBuffers", wglSwapBuffersAddr, &h_wglSwapBuffers, &o_wglSwapBuffers);
            } else {
                console::writeln("Failed to get wglSwapBuffers address");
            }
        } else {
            console::writeln("Failed to get opengl32.dll handle");
        }
    } else if (rcx == 1) {
        console::writeln("Hooking present for DirectX12");
        mem::hooks::addHook("renderer::createDXGIFactory1", tc::offsets::renderer::createDXGIFactory1, &h_createDXGIFactory1, &funcs::renderer::createDXGIFactory1);
    }

    g_prevInitializedFor = rcx;
    return funcs::renderer::initializeForAPI(a1, a2);
}

// Public Functions
//------------------------------------------------------------------------
void renderer::hookPresent() {
    mem::hooks::addHook("renderer::initializeForAPI", tc::offsets::renderer::initializeForAPI, &h_rendererInitFor, &funcs::renderer::initializeForAPI);
}