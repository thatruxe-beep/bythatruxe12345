#include "Gfx/ProtectedOverlay.hpp"

#include <cstring>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace
{
    constexpr wchar_t kOverlayClass[] = L"ForkHackProtectedOverlay";

    HWND gameWindow = nullptr;
    HWND overlayWindow = nullptr;
    IDirect3DDevice9* device = nullptr;

    // Offscreen UI target on GTA's own device. No additional swap chain and
    // no second Present: MTA hooks Present and core.dll treats unexpected
    // graphics state on the device as fatal (the 0xE0000008 RaiseException).
    // The finished frame is downloaded with GetRenderTargetData and given to
    // the capture-protected layered window via UpdateLayeredWindow, which the
    // DWM blends above the game.
    IDirect3DTexture9* uiTexture = nullptr;
    IDirect3DSurface9* uiSurface = nullptr;
    IDirect3DSurface9* downloadSurface = nullptr;
    IDirect3DSurface9* previousRenderTarget = nullptr;
    IDirect3DSurface9* previousDepthStencil = nullptr;

    HDC screenDc = nullptr;
    HDC memoryDc = nullptr;
    HBITMAP dib = nullptr;
    HBITMAP dcDefaultBitmap = nullptr;
    void* dibBits = nullptr;

    int surfaceWidth = 0;
    int surfaceHeight = 0;
    int dibWidth = 0;
    int dibHeight = 0;
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

    void ReleaseD3DResources()
    {
        if (uiSurface)
        {
            uiSurface->Release();
            uiSurface = nullptr;
        }
        if (uiTexture)
        {
            uiTexture->Release();
            uiTexture = nullptr;
        }
        if (downloadSurface)
        {
            downloadSurface->Release();
            downloadSurface = nullptr;
        }
        surfaceWidth = 0;
        surfaceHeight = 0;
    }

    void ReleaseGdi()
    {
        if (memoryDc && dib)
        {
            SelectObject(memoryDc, dcDefaultBitmap);
        }
        if (dib)
        {
            DeleteObject(dib);
            dib = nullptr;
            dibBits = nullptr;
        }
        if (memoryDc)
        {
            DeleteDC(memoryDc);
            memoryDc = nullptr;
        }
        if (screenDc)
        {
            ReleaseDC(nullptr, screenDc);
            screenDc = nullptr;
        }
        dcDefaultBitmap = nullptr;
        dibWidth = 0;
        dibHeight = 0;
    }

    bool EnsureD3DResources(int width, int height)
    {
        if (uiTexture && uiSurface && downloadSurface
            && width == surfaceWidth && height == surfaceHeight)
        {
            return true;
        }

        ReleaseD3DResources();

        if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &uiTexture, nullptr)))
        {
            return false;
        }
        if (FAILED(uiTexture->GetSurfaceLevel(0, &uiSurface)))
        {
            ReleaseD3DResources();
            return false;
        }
        if (FAILED(device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8,
            D3DPOOL_SYSTEMMEMORY, &downloadSurface, nullptr)))
        {
            ReleaseD3DResources();
            return false;
        }

        surfaceWidth = width;
        surfaceHeight = height;
        return true;
    }

    bool EnsureGdiResources(int width, int height)
    {
        if (dib && dibBits && width == dibWidth && height == dibHeight)
        {
            return true;
        }

        if (!screenDc)
        {
            screenDc = GetDC(nullptr);

            if (!screenDc)
            {
                return false;
            }
        }
        if (!memoryDc)
        {
            memoryDc = CreateCompatibleDC(screenDc);

            if (!memoryDc)
            {
                return false;
            }
        }

        if (dib)
        {
            SelectObject(memoryDc, dcDefaultBitmap);
            DeleteObject(dib);
            dib = nullptr;
            dibBits = nullptr;
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; // top-down: first row is the top
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        dib = CreateDIBSection(screenDc, &info, DIB_RGB_COLORS, &dibBits, nullptr, 0);

        if (!dib)
        {
            return false;
        }

        dcDefaultBitmap = (HBITMAP)SelectObject(memoryDc, dib);
        dibWidth = width;
        dibHeight = height;
        return true;
    }

    void UploadFrame(int width, int height)
    {
        if (FAILED(device->GetRenderTargetData(uiSurface, downloadSurface)))
        {
            return;
        }

        D3DLOCKED_RECT locked{};

        if (FAILED(downloadSurface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        {
            return;
        }

        const unsigned char* source = static_cast<const unsigned char*>(locked.pBits);
        unsigned char* target = static_cast<unsigned char*>(dibBits);
        const int sourcePitch = locked.Pitch;
        const int targetPitch = width * 4;

        // Alpha blending the UI onto a target cleared to (0,0,0,0) leaves the
        // color channels premultiplied, which is exactly what
        // UpdateLayeredWindow expects - so this is a plain pitch-aware row
        // copy, no per-pixel work.
        if (sourcePitch == targetPitch)
        {
            std::memcpy(target, source, static_cast<size_t>(targetPitch) * height);
        }
        else
        {
            for (int y = 0; y < height; ++y)
            {
                std::memcpy(
                    target + static_cast<size_t>(y) * targetPitch,
                    source + static_cast<size_t>(y) * sourcePitch,
                    targetPitch);
            }
        }

        downloadSurface->UnlockRect();

        POINT sourcePoint{ 0, 0 };
        SIZE windowSize{ width, height };
        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(overlayWindow, screenDc, nullptr, &windowSize,
            memoryDc, &sourcePoint, 0, &blend, ULW_ALPHA);
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

    // Top-level window on purpose: MTA (CEF/browser, input) walks the game
    // window's children, and a foreign layered child there is asking for
    // trouble. The overlay repositions itself over the client area every
    // frame anyway (see BeginFrame), so no parenting is needed.
    overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayClass,
        L"ForkHack protected overlay",
        WS_POPUP,
        origin.x,
        origin.y,
        width,
        height,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr);

    if (!overlayWindow)
    {
        Shutdown();
        return false;
    }

    // Content arrives with the first UpdateLayeredWindow call: a layered
    // window that has not been given content yet is simply not displayed, so
    // there is no empty-frame flash and no need for SetLayeredWindowAttributes.

    if (!ApplyCaptureProtection(overlayWindow))
    {
        Shutdown();
        return false;
    }

    if (!EnsureGdiResources(width, height) || !EnsureD3DResources(width, height))
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

    if (!EnsureD3DResources(width, height) || !EnsureGdiResources(width, height))
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

    device->SetDepthStencilSurface(nullptr);
    const HRESULT targetResult = device->SetRenderTarget(0, uiSurface);
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
    if (!device || !uiSurface || !downloadSurface || !dibBits)
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

    UploadFrame(surfaceWidth, surfaceHeight);
}

void ProtectedOverlay::BeforeDeviceReset()
{
    ReleaseSavedSurfaces();
    ReleaseD3DResources();
}

void ProtectedOverlay::AfterDeviceReset()
{
    // The render target and download surface live in D3DPOOL_DEFAULT and are
    // released in BeforeDeviceReset; EnsureD3DResources recreates them on the
    // next BeginFrame with the post-reset client size.
}

void ProtectedOverlay::Shutdown()
{
    ReleaseSavedSurfaces();
    ReleaseD3DResources();
    ReleaseGdi();
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
