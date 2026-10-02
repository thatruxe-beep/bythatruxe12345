// 18:32 loader — окно проверки лицензии и обычный запуск GTA: SA.
// Загрузчик не внедряет код в процесс игры и не трогает защитное ПО:
// он проверяет ключ на локальном сервере и запускает gta_sa.exe как есть.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d9.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <cstring>
#include <string>

#include "Hwid.hpp"
#include "Loader.hpp"
#include "Ui.hpp"

#pragma comment(lib, "d3d9.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
LoaderModel g_model;
LoaderController g_controller;
std::string g_iniPath;

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return std::string();
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string narrow(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &narrow[0], length,
                        nullptr, nullptr);
    return narrow;
}

std::string ModuleDirectory()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::string full = WideToUtf8(path);
    const size_t slash = full.find_last_of("\\/");
    return slash != std::string::npos ? full.substr(0, slash) : ".";
}

float QueryScale(HWND window)
{
    HDC dc = GetDC(window);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(window, dc);
    return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
        return 1;

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    // до создания окон: иначе DPI виртуализируется
    SetProcessDPIAware();

    // настройки рядом с exe
    g_iniPath = ModuleDirectory() + "\\loader.ini";
    g_model.machineId = GetMachineId();
    LoadSettings(g_model, g_iniPath);
    if (g_model.gamePath.empty())
        g_model.gamePath = ModuleDirectory() + "\\gta_sa.exe";

    const wchar_t className[] = L"1832LoaderWindow";    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush(RGB(11, 14, 19));
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    // окно фиксированного размера с учётом DPI
    HWND probe = CreateWindowExW(0, className, L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
                                 nullptr, nullptr, instance, nullptr);
    const float scale = QueryScale(probe);
    DestroyWindow(probe);

    const int windowWidth = static_cast<int>(520.0f * scale);
    const int windowHeight = static_cast<int>(486.0f * scale);
    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT area = {0, 0, windowWidth, windowHeight};
    AdjustWindowRect(&area, style, FALSE);
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND window = CreateWindowExW(0, className, L"18:32 loader", style,
                                  (screenWidth - width) / 2, (screenHeight - height) / 2,
                                  width, height, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    // Direct3D 9
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        MessageBoxW(window, L"Не удалось инициализировать Direct3D 9.", L"18:32 loader", MB_ICONERROR);
        return 1;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    IDirect3DDevice9* device = nullptr;
    HRESULT result = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                       D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(result))
    {
        result = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    }
    if (FAILED(result))
    {
        MessageBoxW(window, L"Не удалось создать устройство Direct3D 9.", L"18:32 loader", MB_ICONERROR);
        d3d->Release();
        return 1;
    }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = false;
    SetupLoaderFonts(scale);
    SetupLoaderStyle();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX9_Init(device);

    // если ключ уже сохранён — проверяем его сразу
    if (strlen(g_model.key) >= 4)
        g_controller.Start(g_model, false);

    // главный цикл
    bool done = false;
    MSG msg = {};
    while (!done)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                done = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (done)
            break;

        if (g_controller.Pump(g_model))
            SaveSettings(g_model, g_iniPath);

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();

        RenderLoader(g_model, g_controller, scale);

        ImGui::EndFrame();
        ImGui::Render();

        device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(11, 14, 19), 1.0f, 0);
        if (device->BeginScene() >= 0)
        {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            device->EndScene();
        }

        const HRESULT present = device->Present(nullptr, nullptr, nullptr, nullptr);
        if (present == D3DERR_DEVICELOST &&
            device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
        {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            if (device->Reset(&pp) == D3D_OK)
                ImGui_ImplDX9_CreateDeviceObjects();
        }

        // небольшой сон: интерфейс продолжает анимацию, но не грузит CPU
        Sleep(16);
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    device->Release();
    d3d->Release();
    DestroyWindow(window);
    UnregisterClassW(className, instance);
    return 0;
}
