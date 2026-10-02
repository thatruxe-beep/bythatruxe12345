#include <d3d9.h>

#include <algorithm>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "minhook.hpp"

#include "Utils/xorstr.h"
#include "Utils/D3D9.hpp"

#include "Gfx/Blur.hpp"
#include "Gfx/Fonts.hpp"
#include "Menu/Menu.hpp"
#include "Core/Config.hpp"
#include "Game/Features.h"

#include "Present.hpp"

#include "../core/ForceCursorVisible.hpp"

using tPresent = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
tPresent oPresent = nullptr;

static LPVOID sPresentTarget = nullptr;

static WNDPROC oWndProc = nullptr;
static bool imgui_initialized = false;
static HWND hGameWindow = nullptr;
static volatile LONG sPresentCalls = 0;

namespace
{
    struct PresentCallGuard
    {
        PresentCallGuard() { InterlockedIncrement(&sPresentCalls); }
        ~PresentCallGuard() { InterlockedDecrement(&sPresentCalls); }
    };
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);


LRESULT WINAPI WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool is_open = (menu && menu->GetState());

    if (is_open && ImGui::GetCurrentContext())
    {
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    }

    if (is_open)
    {
        if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
            || (message >= WM_KEYFIRST && message <= WM_KEYLAST)
            || message == WM_INPUT)
        {
            return 1;
        }
    }

    return oWndProc
        ? CallWindowProcW(oWndProc, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

static void InitImGui(IDirect3DDevice9* device)
{
    D3DDEVICE_CREATION_PARAMETERS deviceParameters;
    device->GetCreationParameters(&deviceParameters);

    oWndProc = (WNDPROC)SetWindowLongPtrW(deviceParameters.hFocusWindow, GWLP_WNDPROC, (LONG_PTR)WndProcHandler);
    hGameWindow = deviceParameters.hFocusWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui::StyleColorsDark();
    Fonts::InitStyle();
    ImGui::GetStyle().AntiAliasedLines = false;
    ImGui::GetStyle().AntiAliasedFill = false;
    g_fonts.Init();

    ImGui_ImplWin32_Init(deviceParameters.hFocusWindow);
    ImGui_ImplDX9_Init(device);
    Blur::SetDevice(device);
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* self, const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion)
{
    PresentCallGuard callGuard;

    if (!imgui_initialized)
    {
        InitImGui(self);
        imgui_initialized = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().FontGlobalScale = g_cfg.ui_scale / 100.0f;

    KeyBinds::Update();
    World::Update();
    RapidFire::Update();
    GameSpeed::Update();
    FastRot::Update();
    VehicleFlags::Update();
    NoBikeFall::Update();
    Aspect::Update();
    Fov::Update();
    AutoEngine::Update();
    AutoUnlock::Update();
    FastCrosshair::Update();
    NoRecoil::Update();
    NoSpread::Update();
    Triggerbot::Update();
    NoCamCol::Update();
    CamHack::Update();

    Esp::Update();

    GodMode::Update();
    NoFall::Update();
    FastRun::Update();
    SpeedHack::Update();
    Blur::NewFrame();

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        menu->ToggleState();

        if (Cself)
        {
            if (menu->GetState())
            {
                callForceCursorVisible(Cself, true, true);
            }
            else
            {
                callForceCursorVisible(Cself, false, false);
            }
        }
    }

    menu->Draw();
    menu->DrawBinds();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    const bool is_open = menu->GetState();

    if (is_open && hGameWindow)
    {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hGameWindow, &p);
        ImDrawList* cursorList = ImGui::GetForegroundDrawList();
        const float cursorScale = g_cfg.ui_scale / 100.0f;
        const ImVec2 origin((float)p.x, (float)p.y);
        const ImU32 accent = g_cfg.accent.to_color().u32();
        ImVec2 cursorShape[] = {
            origin,
            origin + ImVec2(2.8f, 18.0f) * cursorScale,
            origin + ImVec2(7.0f, 13.8f) * cursorScale,
            origin + ImVec2(11.2f, 22.0f) * cursorScale,
            origin + ImVec2(15.0f, 20.0f) * cursorScale,
            origin + ImVec2(10.8f, 12.0f) * cursorScale,
            origin + ImVec2(17.0f, 10.0f) * cursorScale
        };
        ImVec2 cursorShadow[7];
        for (int i = 0; i < 7; ++i)
        {
            cursorShadow[i] = cursorShape[i] + ImVec2(2.0f, 2.0f) * cursorScale;
        }
        cursorList->AddConvexPolyFilled(cursorShadow, 7, IM_COL32(0, 0, 0, 150));
        cursorList->AddConvexPolyFilled(cursorShape, 7, IM_COL32(25, 25, 25, 255));
        cursorList->AddPolyline(cursorShape, 7, accent, true, 1.7f * cursorScale);
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oPresent(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
}

void Present::InstallHook()
{
    MH_STATUS status;

    sPresentTarget = Utils::get_function_address(17);

    if (!sPresentTarget)
    {
        MessageBoxA(NULL, "Failed to find device for d3d9/present", "18:32 cheat", MB_OK | MB_ICONERROR);
        return;
    }

    status = MH_CreateHook(sPresentTarget, &hkPresent, reinterpret_cast<LPVOID*>(&oPresent));
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to create hook on function d3d9/present", "18:32 cheat", MB_OK | MB_ICONERROR);
    }

    status = MH_EnableHook(sPresentTarget);
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to enable hook on function d3d9/present", "18:32 cheat", MB_OK | MB_ICONERROR);
    }
}

void Present::RemoveHook()
{
    MH_STATUS status;

    if (sPresentTarget == nullptr)
    {
        return;
    }

    status = MH_DisableHook(sPresentTarget);
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to disable hook on function d3d9/present", "18:32 cheat", MB_OK | MB_ICONERROR);
    }

    for (int i = 0; i < 2000 && InterlockedCompareExchange(&sPresentCalls, 0, 0) != 0; ++i)
    {
        Sleep(1);
    }

    status = MH_RemoveHook(sPresentTarget);
    if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
    {
        MessageBoxA(NULL, "Failed to remove hook on function d3d9/present", "18:32 cheat", MB_OK | MB_ICONERROR);
    }
    sPresentTarget = nullptr;
    oPresent = nullptr;
}

void Present::Shutdown()
{
    if (!imgui_initialized)
    {
        return;
    }

    if (hGameWindow && oWndProc)
    {
        const auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hGameWindow, GWLP_WNDPROC));
        if (current == WndProcHandler)
        {
            SetWindowLongPtrW(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }
    }

    if (menu)
    {
        menu->ReleaseTextures();
    }
    Blur::ClearTextures();

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    oWndProc = nullptr;
    hGameWindow = nullptr;
    imgui_initialized = false;
}
