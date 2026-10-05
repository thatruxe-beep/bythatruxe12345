#include "Gfx/ProtectedOverlay.hpp"

#include <dwmapi.h>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace
{
    constexpr wchar_t kOverlayClass[] = L"ForkHackProtectedOverlay";

    HWND gameWindow = nullptr;
    HWND overlayWindow = nullptr;
    IDirect3DDevice9* device = nullptr;
    IDirect3DSwapChain9* swapChain = nullptr;
    IDirect3DSurface9* previousRenderTarget = nullptr;
    IDirect3DSurface9* previousDepthStencil = nullptr;
    D3DPRESENT_PARAMETERS params{};
    D3DFORMAT fallbackFormat = D3DFMT_X8R8G8B8;
    int currentWidth = 0;
    int currentHeight = 0;
    bool classRegistered = false;

    LRESULT CALLBACK OverlayWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_ERASEBKGND)
        {
            return 1;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool GetGameClientBounds(POINT& origin, int& width, int& height)
    {
        RECT client{};
        if (!gameWindow || !IsWindow(gameWindow) || !GetClientRect(gameWindow, &client))
        {
            return false;
        }

        origin = { client.left, client.top };
        if (!ClientToScreen(gameWindow, &origin))
        {
            return false;
        }

        width = client.right - client.left;
        height = client.bottom - client.top;
        return width > 0 && height > 0;
    }

    bool ApplyCaptureProtection(HWND window)
    {
        return SetWindowDisplayAffinity(window, WDA_EXCLUDEFROMCAPTURE) != FALSE;
    }

    void ReleaseSavedSurfaces()
    {
        if (previousRenderTarget)
        {
            previousRenderTarget->Release();
            previousRenderTarget = nullptr;
        }
        if (previousDepthStencil)
        {
            previousDepthStencil->Release();
            previousDepthStencil = nullptr;
        }
    }

    void ReleaseSwapChain()
    {
        ReleaseSavedSurfaces();
        if (swapChain)
        {
            swapChain->Release();
            swapChain = nullptr;
        }
    }

    bool CreateSwapChain(int width, int height)
    {
        if (!device || !overlayWindow || width <= 0 || height <= 0)
        {
            return false;
        }

        ReleaseSwapChain();

        params.BackBufferWidth = width;
        params.BackBufferHeight = height;
        params.hDeviceWindow = overlayWindow;
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.MultiSampleType = D3DMULTISAMPLE_NONE;
        params.MultiSampleQuality = 0;
        params.EnableAutoDepthStencil = FALSE;
        params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
        params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        params.BackBufferFormat = D3DFMT_A8R8G8B8;

        HRESULT result = device->CreateAdditionalSwapChain(&params, &swapChain);
        if (FAILED(result))
        {
            // Some older D3D9 drivers reject A8R8G8B8 for an additional chain.
            // The DWM glass surface remains transparent with the primary format.
            params.BackBufferFormat = fallbackFormat;
            result = device->CreateAdditionalSwapChain(&params, &swapChain);
        }

        if (FAILED(result))
        {
            return false;
        }

        currentWidth = width;
        currentHeight = height;
        return true;
    }
}

bool ProtectedOverlay::Initialize(HWND targetGameWindow, IDirect3DDevice9* gameDevice)
{
    gameWindow = targetGameWindow;
    device = gameDevice;
    if (!gameWindow || !device)
    {
        return false;
    }
    device->AddRef();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    windowClass.lpszClassName = kOverlayClass;

    classRegistered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    if (!classRegistered)
    {
        Shutdown();
        return false;
    }

    POINT origin{};
    int width = 0;
    int height = 0;
    if (!GetGameClientBounds(origin, width, height))
    {
        Shutdown();
        return false;
    }

    overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayClass,
        L"ForkHack protected overlay",
        WS_POPUP,
        origin.x,
        origin.y,
        width,
        height,
        gameWindow,
        nullptr,
        windowClass.hInstance,
        nullptr);

    if (!overlayWindow)
    {
        Shutdown();
        return false;
    }

    SetLayeredWindowAttributes(overlayWindow, 0, 255, LWA_ALPHA);

    MARGINS margins{ -1, -1, -1, -1 };
    if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
    {
        using ExtendFrameFn = HRESULT(WINAPI*)(HWND, const MARGINS*);
        if (auto extendFrame = reinterpret_cast<ExtendFrameFn>(GetProcAddress(dwm, "DwmExtendFrameIntoClientArea")))
        {
            extendFrame(overlayWindow, &margins);
        }
        FreeLibrary(dwm);
    }

    if (!ApplyCaptureProtection(overlayWindow))
    {
        Shutdown();
        return false;
    }

    // Reuse the primary chain's driver-compatible format and flags as fallback.
    if (IDirect3DSwapChain9* primary = nullptr; SUCCEEDED(device->GetSwapChain(0, &primary)))
    {
        D3DPRESENT_PARAMETERS primaryParams{};
        if (SUCCEEDED(primary->GetPresentParameters(&primaryParams)) && primaryParams.BackBufferFormat != D3DFMT_UNKNOWN)
        {
            fallbackFormat = primaryParams.BackBufferFormat;
        }
        primary->Release();
    }

    if (!CreateSwapChain(width, height))
    {
        Shutdown();
        return false;
    }

    ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
    UpdateWindow(overlayWindow);
    return true;
}

bool ProtectedOverlay::BeginFrame()
{
    if (!device || !overlayWindow)
    {
        return false;
    }

    POINT origin{};
    int width = 0;
    int height = 0;
    if (!GetGameClientBounds(origin, width, height))
    {
        ShowWindow(overlayWindow, SW_HIDE);
        return false;
    }

    const bool gameIsForeground = GetForegroundWindow() == gameWindow;
    if (IsIconic(gameWindow) || !IsWindowVisible(gameWindow) || !gameIsForeground)
    {
        ShowWindow(overlayWindow, SW_HIDE);
        return false;
    }

    SetWindowPos(
        overlayWindow,
        HWND_TOPMOST,
        origin.x,
        origin.y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if ((!swapChain || width != currentWidth || height != currentHeight) && !CreateSwapChain(width, height))
    {
        return false;
    }

    ReleaseSavedSurfaces();
    if (FAILED(device->GetRenderTarget(0, &previousRenderTarget)))
    {
        return false;
    }
    // A device is allowed to have no depth-stencil surface.
    device->GetDepthStencilSurface(&previousDepthStencil);

    IDirect3DSurface9* overlayBackBuffer = nullptr;
    if (FAILED(swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &overlayBackBuffer)))
    {
        ReleaseSavedSurfaces();
        return false;
    }

    device->SetDepthStencilSurface(nullptr);
    const HRESULT targetResult = device->SetRenderTarget(0, overlayBackBuffer);
    overlayBackBuffer->Release();
    if (FAILED(targetResult))
    {
        if (previousDepthStencil)
        {
            device->SetDepthStencilSurface(previousDepthStencil);
        }
        ReleaseSavedSurfaces();
        return false;
    }

    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    return true;
}

void ProtectedOverlay::EndFrame()
{
    if (!device || !swapChain)
    {
        ReleaseSavedSurfaces();
        return;
    }

    if (previousRenderTarget)
    {
        device->SetRenderTarget(0, previousRenderTarget);
    }
    if (previousDepthStencil)
    {
        device->SetDepthStencilSurface(previousDepthStencil);
    }
    ReleaseSavedSurfaces();

    swapChain->Present(nullptr, nullptr, overlayWindow, nullptr, 0);
}

void ProtectedOverlay::BeforeDeviceReset()
{
    ReleaseSwapChain();
}

void ProtectedOverlay::AfterDeviceReset()
{
    POINT origin{};
    int width = 0;
    int height = 0;
    if (GetGameClientBounds(origin, width, height))
    {
        CreateSwapChain(width, height);
    }
}

void ProtectedOverlay::Shutdown()
{
    ReleaseSwapChain();
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    if (overlayWindow)
    {
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
    if (classRegistered)
    {
        UnregisterClassW(kOverlayClass, GetModuleHandleW(nullptr));
        classRegistered = false;
    }
}

HWND ProtectedOverlay::GetWindow()
{
    return overlayWindow;
}

IDirect3DDevice9* ProtectedOverlay::GetDevice()
{
    return device;
}

void ProtectedOverlay::SetVisible(bool visible)
{
    if (overlayWindow)
    {
        ShowWindow(overlayWindow, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}
