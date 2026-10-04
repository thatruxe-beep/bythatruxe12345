#include "Menu/Menu.hpp"

#include "Game/Features.h"

#include "Gfx/Blur.hpp"
#include "Gfx/Fonts.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    // Стрелки 18:32 — маленький фирменный циферблат.
    void DrawClock(ImDrawList* list, const ImVec2& center, float radius,
        ImU32 accent, ImU32 dim, float thickness)
    {
        list->AddCircle(center, radius, dim, 24, thickness);

        const float hourAngle = (18.0f + 32.0f / 60.0f) / 12.0f * 2.0f * IM_PI;
        const float minuteAngle = 32.0f / 60.0f * 2.0f * IM_PI;

        // Угол отсчитывается от 12 часов, ось Y экрана направлена вниз.
        const ImVec2 hourEnd(
            center.x + std::sin(hourAngle) * radius * 0.52f,
            center.y - std::cos(hourAngle) * radius * 0.52f);
        const ImVec2 minuteEnd(
            center.x + std::sin(minuteAngle) * radius * 0.82f,
            center.y - std::cos(minuteAngle) * radius * 0.82f);

        list->AddLine(center, hourEnd, accent, thickness);
        list->AddLine(center, minuteEnd, accent, thickness);
        list->AddCircleFilled(center, radius * 0.10f, accent, 8);
    }

    // Имя виртуальной клавиши для бейджа.
    const char* KeyName(int vk)
    {
        if (vk == 0x01) return "LMB";
        if (vk == 0x02) return "RMB";
        if (vk == 0x04) return "MMB";
        if (vk == 0x05) return "MB4";
        if (vk == 0x06) return "MB5";
        if (vk == 0x08) return "BKSP";
        if (vk == 0x09) return "TAB";
        if (vk == 0x0D) return "ENTER";
        if (vk == 0x10) return "SHIFT";
        if (vk == 0x11) return "CTRL";
        if (vk == 0x12) return "ALT";
        if (vk == 0x14) return "CAPS";
        if (vk == 0x1B) return "ESC";
        if (vk == 0x20) return "SPACE";
        if (vk == 0x21) return "PGUP";
        if (vk == 0x22) return "PGDN";
        if (vk == 0x23) return "END";
        if (vk == 0x24) return "HOME";
        if (vk == 0x25) return "LEFT";
        if (vk == 0x26) return "UP";
        if (vk == 0x27) return "RIGHT";
        if (vk == 0x28) return "DOWN";
        if (vk == 0x2D) return "INS";
        if (vk == 0x2E) return "DEL";
        if (vk >= 0x30 && vk <= 0x39) return "0123456789" + (vk - 0x30);
        if (vk >= 0x41 && vk <= 0x5A) return "ABCDEFGHIJKLMNOPQRSTUVWXYZ" + (vk - 0x41);
        if (vk >= 0x70 && vk <= 0x7B)
        {
            static char fn[4]{};
            snprintf(fn, sizeof(fn), "F%d", vk - 0x6F);
            return fn;
        }
        if (vk == 0xA0) return "LSHFT";
        if (vk == 0xA1) return "RSHFT";
        if (vk == 0xA2) return "LCTRL";
        if (vk == 0xA3) return "RCTRL";
        if (vk == 0xA4) return "LALT";
        if (vk == 0xA5) return "RALT";
        return "???";
    }

    // Клавиша-клавиатура: холд — залита акцентом, тогл — контурная.
    void DrawKeycap(ImDrawList* list, const ImVec2& min, const ImVec2& max,
        const char* text, bool hold, ImU32 accent, float alpha)
    {
        if (hold)
        {
            list->AddRectFilled(min, max, accent, 3.0f);
        }
        else
        {
            list->AddRectFilled(min, max, IM_COL32(16, 17, 21, (int)(200 * alpha)), 3.0f);
            list->AddRect(min, max, IM_COL32(255, 255, 255, (int)(60 * alpha)), 3.0f);
        }

        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 pos(
            min.x + (max.x - min.x - size.x) * 0.5f,
            min.y + (max.y - min.y - size.y) * 0.5f - 1.0f);
        list->AddText(pos, hold ? IM_COL32(12, 13, 16, (int)(255 * alpha))
            : IM_COL32(235, 235, 240, (int)(235 * alpha)), text);
    }
}

static std::string Bind_Display_Name(int i)
{
    switch (i)
    {
    case 0: return tr("Годмод", "Godmode");
    case 1: return tr("Быстрый бег", "Fast run");
    case 2: return tr("Валлхак", "Wallhack");
    case 3: return tr("Спидхак", "Speedhack");
    case 4: return tr("Аирбрейк", "Airbreak");
    case 5: return tr("Поменять скин", "Change skin");
    case 6: return tr("Найтмод", "Nightmode");
    case 7: return tr("Своё время", "Custom time");
    case 8: return tr("Свет и вода", "Light & water");
    case 9: return tr("Небо", "Sky");
    case 10: return tr("Рапид", "Rapid fire");
    case 11: return tr("Скорость игры", "Game speed");
    case 12: return tr("Быстрая ротация", "Fast Rotation");
    case 13: return tr("Не падать с байка", "No bike fall");
    case 14: return tr("Машины по воде", "Drive on water");
    case 15: return tr("Летающие машины", "Flying cars");
    case 16: return tr("Анти-колизия", "No collision");
    case 17: return tr("Чистка мира", "World Removals");
    case 18: return tr("Свой цвет", "Custom color");
    case 19: return tr("Фулбрайт", "Fullbright");
    case 20: return tr("Аспект", "Aspect");
    case 21: return tr("Автозавод", "Auto engine");
    case 22: return tr("Автооткрытие", "Auto unlock");
    case 23: return tr("Быстрый прицел", "Fast crosshair");
    case 24: return tr("Без отдачи", "No recoil");
    case 25: return tr("Без разброса", "No spread");
    case 26: return tr("Фов", "Fov");
    case 27: return tr("Таран", "Ram");
    case 28: return tr("Заморозить время", "Freeze time");
    case 29: return tr("Туман", "Fog");
    case 30: return tr("Солнце", "Sun");
    case 31: return tr("Триггербот", "Triggerbot");
    case 32: return tr("Без колизии камеры", "No camera collision");
    case 33: return tr("Камхак", "Camhack");
    case 34: return tr("Без урона от падения", "No fall damage");
    case 35: return tr("Рандом ГМ", "Random god mode");
    case 36: return tr("ГМ авто", "Vehicle GM");
    case 37: return tr("Новый рапид", "New rapid");
    case 38: return tr("Клик-варп", "Click warp");
    case 39: return tr("ТП на метку", "TP to marker");
    default: return "";
    }
}

void Menu::DrawBinds()
{
    if (!g_cfg.showbinds)
    {
        return;
    }

    struct bind_ref_t
    {
        keybind_t* bind;
        bool* value;
        bool vehicle;
    };

    static const bind_ref_t kBinds[] = {
        { &g_cfg.godmode_bind, &g_cfg.godmode, false },
        { &g_cfg.fastbeg_bind, &g_cfg.fastbeg, false },
        { &g_cfg.wh_bind, &g_cfg.wh, false },
        { &g_cfg.speedhack_bind, &g_cfg.speedhack, true },
        { &g_cfg.airbreake_bind, &g_cfg.airbreake, false },
        { &g_cfg.changemodel_bind, &g_cfg.changemodel, false },
        { &g_cfg.nightmode_bind, &g_cfg.nightmode, false },
        { &g_cfg.customtime_bind, &g_cfg.customtime, false },
        { &g_cfg.customcolor_bind, &g_cfg.customcolor, false },
        { &g_cfg.skychange_bind, &g_cfg.skychange, false },
        { &g_cfg.rapidfire_bind, &g_cfg.rapidfire, false },
        { &g_cfg.gamespeed_bind, &g_cfg.gamespeed, false },
        { &g_cfg.fastrot_bind, &g_cfg.fastrot, false },
        { &g_cfg.nobikefall_bind, &g_cfg.nobikefall, true },
        { &g_cfg.waterdrive_bind, &g_cfg.waterdrive, true },
        { &g_cfg.carfly_bind, &g_cfg.carfly, true },
        { &g_cfg.nocol_bind, &g_cfg.nocol, false },
        { &g_cfg.removals_bind, &g_cfg.removals, false },
        { &g_cfg.carcolor_bind, &g_cfg.carcolor, true },
        { &g_cfg.fullbright_bind, &g_cfg.fullbright, false },
        { &g_cfg.aspect_bind, &g_cfg.aspect, false },
        { &g_cfg.autoengine_bind, &g_cfg.autoengine, true },
        { &g_cfg.autounlock_bind, &g_cfg.autounlock, false },
        { &g_cfg.fastcross_bind, &g_cfg.fastcross, false },
        { &g_cfg.norecoil_bind, &g_cfg.norecoil, false },
        { &g_cfg.nospread_bind, &g_cfg.nospread, false },
        { &g_cfg.fov_bind, &g_cfg.fov, false },
        { &g_cfg.ram_bind, &g_cfg.ram, true },
        { &g_cfg.freezetime_bind, &g_cfg.freezetime, false },
        { &g_cfg.fogchange_bind, &g_cfg.fogchange, false },
        { &g_cfg.suncolor_bind, &g_cfg.suncolor, false },
        { &g_cfg.trigger_bind, &g_cfg.trigger, false },
        { &g_cfg.nocamcol_bind, &g_cfg.nocamcol, false },
        { &g_cfg.camhack_bind, &g_cfg.camhack, false },
        { &g_cfg.nofall_bind, &g_cfg.nofall, false },
        { &g_cfg.randomgodmode_bind, &g_cfg.randomgodmode, false },
        { &g_cfg.autorepair_bind, &g_cfg.autorepair, true },
        { &g_cfg.newrapid_bind, &g_cfg.newrapid, false },
        { &g_cfg.clickwarp_bind, &g_cfg.clickwarp, false },
        { &g_cfg.tpmarker_bind, &g_cfg.tpmarker, false },
    };

    static const int kBindCount = sizeof(kBinds) / sizeof(kBinds[0]);

    CPed* pBindLocal = FindPlayerPed();
    bool inVehicle = pBindLocal && pBindLocal->m_pVehicle != nullptr;

    int vis[64];
    int nvis = 0;

    for (int i = 0; i < kBindCount && nvis < 64; i++)
    {
        if (kBinds[i].bind->key < 0)
        {
            continue;
        }

        if (kBinds[i].vehicle && !inVehicle)
        {
            continue;
        }

        bool on = kBinds[i].bind->mode == 1 ? ((GetAsyncKeyState(kBinds[i].bind->key) & 0x8000) != 0) : *kBinds[i].value;

        if (on)
        {
            vis[nvis++] = i;
        }
    }

    static float bind_alpha = 0.0f;
    CreateAnimation(bind_alpha, g_cfg.menu_open || nvis > 0, 1.0f, AnimLerp);

    if (bind_alpha <= 0.0f)
    {
        return;
    }

    const float s = GetScale();
    const float row_h = 24.0f * s;
    const float head_h = 36.0f * s;
    const float rows_top = 42.0f * s;
    const float pad_l = 14.0f * s;
    const float pad_r = 12.0f * s;
    const float gap = 10.0f * s;

    ImGui::PushFont(g_fonts.main);

    float max_name = 0.0f;
    float max_key = 0.0f;

    for (int vi = 0; vi < nvis; vi++)
    {
        int i = vis[vi];
        max_name = ImMax(max_name, ImGui::CalcTextSize(Bind_Display_Name(i).c_str()).x);
        max_key = ImMax(max_key, ImGui::CalcTextSize(KeyName(kBinds[i].bind->key)).x);
    }

    const float keycap_w = ImMax(max_key + 14.0f * s, 34.0f * s);
    const float content_w = ImMax(pad_l + max_name + gap + keycap_w + pad_r, 170.0f * s);
    const float win_w = content_w + 8.0f * s;

    static float win_h_anim = 40.0f;
    float win_h_target = rows_top + row_h * nvis + 4.0f * s;
    float win_t = ImGui::GetIO().DeltaTime * 10.0f;

    if (win_t > 1.0f)
    {
        win_t = 1.0f;
    }

    win_h_anim += (win_h_target - win_h_anim) * win_t;

    static bool opened = true;
    static bool pos_set = false;

    if (!pos_set)
    {
        float y = g_cfg.binds_y < 0.0f ? ImGui::GetIO().DisplaySize.y * 0.5f : g_cfg.binds_y;
        ImGui::SetNextWindowPos(ImVec2(g_cfg.binds_x, y));
        pos_set = true;
    }

    ImGui::SetNextWindowSize(ImVec2(win_w, win_h_anim));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##binds_window", &opened, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);

    ImDrawList* list = ImGui::GetWindowDrawList();
    list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    const c_color accent = g_cfg.accent.to_color();
    const ImU32 accentU32 = static_cast<ImU32>(accent.new_alpha((int)(255.0f * bind_alpha)).as_imcolor());
    const float a = bind_alpha;

    ImVec2 window_pos = ImGui::GetWindowPos() + ImVec2(4.0f * s, 1.0f * s);
    float window_alpha = 255.0f * bind_alpha;
    float win_h = win_h_anim;

    static float save_cd = 0.0f;
    ImVec2 cur_pos = ImGui::GetWindowPos();

    if (fabsf(cur_pos.x - g_cfg.binds_x) > 0.5f || fabsf(cur_pos.y - g_cfg.binds_y) > 0.5f)
    {
        g_cfg.binds_x = cur_pos.x;
        g_cfg.binds_y = cur_pos.y;
        save_cd = 2.0f;
    }

    if (save_cd > 0.0f)
    {
        save_cd -= ImGui::GetIO().DeltaTime;

        if (save_cd <= 0.0f)
        {
            SaveGeneralConfig();
        }
    }

    // Панель: тёмная подложка со скруглением 3 и тонкая рамка с акцентной
    // акцентная левая кромка.
    Blur::Create(list, window_pos, window_pos + ImVec2(content_w, win_h),
        ImColor(70, 70, 74, (int)window_alpha), 3.0f * s, 15);
    list->AddRect(window_pos, window_pos + ImVec2(content_w, win_h),
        IM_COL32(90, 90, 96, (int)(110 * a)), 3.0f * s);
    list->AddRectFilled(window_pos, window_pos + ImVec2(3.0f * s, win_h), accentU32);

    // Шапка: циферблат 18:32 + счётчик активных биндов.
    DrawClock(list, window_pos + ImVec2(16.0f * s, head_h * 0.5f), 7.5f * s,
        accentU32, IM_COL32(255, 255, 255, (int)(150 * a)), 1.6f * s);

    const std::string title = "18:32";
    const ImVec2 title_size = ImGui::CalcTextSize(title.c_str());
    list->AddText(ImVec2(window_pos.x + 29.0f * s, window_pos.y + (head_h - title_size.y) * 0.5f),
        c_color(245, 245, 248, (int)(255 * a)).as_imcolor(), title.c_str());

    char counter[16]{};
    snprintf(counter, sizeof(counter), "x%d", nvis);
    const ImVec2 counter_size = ImGui::CalcTextSize(counter);
    list->AddText(ImVec2(window_pos.x + content_w - pad_r - counter_size.x,
        window_pos.y + (head_h - counter_size.y) * 0.5f),
        accentU32, counter);

    list->AddLine(window_pos + ImVec2(0, head_h), window_pos + ImVec2(content_w, head_h),
        IM_COL32(255, 255, 255, (int)(14 * a)));

    // Строки: имя слева, клавиша-клавиатура справа.
    for (int vi = 0; vi < nvis; vi++)
    {
        int i = vis[vi];
        const std::string name = Bind_Display_Name(i);
        const float row_y = window_pos.y + rows_top + row_h * vi;
        const float text_y = row_y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;

        if (vi > 0)
        {
            list->AddLine(
                ImVec2(window_pos.x + pad_l * 0.5f, row_y),
                ImVec2(window_pos.x + content_w - pad_r * 0.5f, row_y),
                IM_COL32(255, 255, 255, (int)(9 * a)));
        }

        list->AddText(ImVec2(window_pos.x + pad_l, text_y),
            c_color(238, 238, 242, (int)(245 * a)).as_imcolor(), name.c_str());

        const char* key = KeyName(kBinds[i].bind->key);
        const bool hold = kBinds[i].bind->mode == 1;
        const float cap_h = 16.0f * s;
        DrawKeycap(list,
            ImVec2(window_pos.x + content_w - pad_r - keycap_w, row_y + (row_h - cap_h) * 0.5f),
            ImVec2(window_pos.x + content_w - pad_r, row_y + (row_h + cap_h) * 0.5f),
            key, hold, accentU32, a);
    }

    list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

    ImGui::End(false);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}
