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
#include "Menu/Auth.hpp"
#include "Core/Config.hpp"
#include "Core/License.hpp"
#include "Core/Runtime.hpp"
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
static volatile LONG sWndProcCalls = 0;
static volatile LONG sShuttingDown = 0;
static volatile LONG sUnloadRequested = 0;

namespace
{
    struct PresentCallGuard
    {
        PresentCallGuard() { InterlockedIncrement(&sPresentCalls); }
        ~PresentCallGuard() { InterlockedDecrement(&sPresentCalls); }
    };

    struct WndProcCallGuard
    {
        WndProcCallGuard() { InterlockedIncrement(&sWndProcCalls); }
        ~WndProcCallGuard() { InterlockedDecrement(&sWndProcCalls); }
    };
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);


LRESULT WINAPI WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    WndProcCallGuard callGuard;
    WNDPROC original = oWndProc;
    const bool shuttingDown = InterlockedCompareExchange(&sShuttingDown, 0, 0) != 0;
    // Окно активации перехватывает ввод так же, как открытое меню.
    const bool auth_active = !shuttingDown && License::NeedsInputOverlay();
    const bool is_open = !shuttingDown && !auth_active && (menu && menu->GetState());
    const bool wants_input = auth_active || is_open;

    if (wants_input && ImGui::GetCurrentContext())
    {
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    }

    if (wants_input)
    {
        if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
            || (message >= WM_KEYFIRST && message <= WM_KEYLAST)
            || message == WM_INPUT)
        {
            return 1;
        }
    }

    return original
        ? CallWindowProcW(original, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

static void RestoreWindowProcedure()
{
    InterlockedExchange(&sShuttingDown, 1);

    if (hGameWindow && oWndProc)
    {
        const auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hGameWindow, GWLP_WNDPROC));
        if (current == WndProcHandler)
        {
            SetWindowLongPtrW(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }
    }

    // Once the original procedure is restored, no new calls can enter our
    // handler. Wait for messages already inside it before unloading code.
    for (int i = 0; i < 2000 && InterlockedCompareExchange(&sWndProcCalls, 0, 0) != 0; ++i)
    {
        Sleep(1);
    }
}

static void InitImGui(IDirect3DDevice9* device)
{
    D3DDEVICE_CREATION_PARAMETERS deviceParameters;
    device->GetCreationParameters(&deviceParameters);

    InterlockedExchange(&sShuttingDown, 0);
    InterlockedExchange(&sWndProcCalls, 0);
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

    if (InterlockedCompareExchange(&sShuttingDown, 0, 0) != 0)
    {
        return oPresent(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
    }

    // Finish the frame in which the button was clicked, then perform all MTA
    // cursor and USER32 work here at the start of the next render frame.
    if (InterlockedExchange(&sUnloadRequested, 0) != 0)
    {
        g_cfg.menu_open = false;
        if (Cself && callForceCursorVisible)
        {
            callForceCursorVisible(Cself, false, false);
        }
        RestoreWindowProcedure();
        Runtime::RequestUnload();
        return oPresent(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
    }

    if (!imgui_initialized)
    {
        InitImGui(self);
        imgui_initialized = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().FontGlobalScale = g_cfg.ui_scale / 100.0f;

    // Периодическая проверка срока (истёкший ключ отключает функции).
    License::Tick();

    if (!License::Authorized())
    {
        // До активации лицензии ни одна функция чита не работает:
        // рисуем окно ввода ключа вместо меню.
        Auth::Draw();
        License::EnforceExit();

        // Пусть ImGui сам показывает системный курсор-стрелку.
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        return oPresent(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
    }

    KeyBinds::Update();
    World::Update();
    RapidFire::Update();
    NewRapid::Update();
    GameSpeed::Update();
    FastRot::Update();
    VehicleFlags::Update();
    NoBikeFall::Update();
    Aspect::Update();
    Fov::Update();
    AutoEngine::Update();
    AutoUnlock::Update();
    AutoRepair::Update();
    Ram::Update();
    FastCrosshair::Update();
    NoRecoil::Update();
    NoSpread::Update();
    Triggerbot::Update();
    NoCamCol::Update();
    CamHack::Update();

    Esp::Update();

    GodMode::Update();
    RandomGodMode::Update();
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
        const float cursorScale = g_cfg.ui_scale / 100.0f;
        const ImVec2 origin((float)p.x, (float)p.y);
        const ImU32 accent = static_cast<ImU32>(g_cfg.accent.to_color().as_imcolor());

        // Use ImGui's standard arrow geometry (matching a regular system cursor)
        // and only replace its white fill with the configured accent color.
        ImGui::RenderMouseCursor(ImGui::GetForegroundDrawList(), origin, cursorScale,
            ImGuiMouseCursor_Arrow, accent, IM_COL32(15, 15, 15, 255), IM_COL32(0, 0, 0, 80));
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oPresent(self, sourceRect, destRect, destWindowOverride, dirtyRegion);
}

void Present::RequestUnload()
{
    InterlockedExchange(&sUnloadRequested, 1);
}

void Present::InstallHook()
{
    MH_STATUS status;
    InterlockedExchange(&sUnloadRequested, 0);

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
        RestoreWindowProcedure();
        return;
    }

    // Detach USER32 from DLL code before stopping rendering or destroying ImGui.
    RestoreWindowProcedure();

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

    RestoreWindowProcedure();

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
