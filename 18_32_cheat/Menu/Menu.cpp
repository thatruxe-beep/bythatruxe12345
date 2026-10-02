#include "Menu/Menu.hpp"

#include "Utils/xorstr.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <shellapi.h>

#include "Assets/Assets.hpp"
#include "Gfx/Blur.hpp"
#include "Gfx/Fonts.hpp"
#include "Gfx/Texture.hpp"

#include "Game/Features.h"

Menu* menu = new Menu;

static bool s_dragging = false;
static ImVec2 s_drag_offset{};
static bool s_drag_pending = false;
static ImVec2 s_drag_pending_pos{};

uint32_t Menu::Hash_Label(const char* text)
{
    uint32_t hash = 2166136261u;

    while (*text)
    {
        hash ^= (uint8_t)(*text);
        hash *= 16777619u;
        text++;
    }

    return hash;
}

bool Menu::GetState()
{
    return g_cfg.menu_open;
}

bool Menu::ToggleState()
{
    g_cfg.menu_open = !g_cfg.menu_open;

    return g_cfg.menu_open;
}

void Menu::SetDrawList(ImDrawList* list)
{
    draw_list = list;
}

void Menu::SetWindowPos(const ImVec2& pos)
{
    window_pos = pos;
}

ImVec2 Menu::GetWindowPos()
{
    const float s = GetScale();
    return window_pos + ImVec2(45.0f * s, 15.0f * s);
}

void Menu::CreateAnimation(float& mod, bool cond, float speed, unsigned int flags)
{
    float time = (ImGui::GetIO().DeltaTime * 5.0f * 5.0f) * speed;

    if ((flags & AnimSkipEnable) && cond)
    {
        mod = 1.0f;
    }
    else if ((flags & AnimSkipDisable) && !cond)
    {
        mod = 0.0f;
    }

    if (flags & AnimLerp)
    {
        mod = mod + ((float)cond - mod) * time;
    }
    else
    {
        if (cond && mod <= 1.0f)
        {
            mod += time;
        }
        else if (!cond && mod >= 0.0f)
        {
            mod -= time;
        }
    }

    if (mod < 0.0f)
    {
        mod = 0.0f;
    }

    if (mod > 1.0f)
    {
        mod = 1.0f;
    }
}

void Menu::UpdateAlpha()
{
    CreateAnimation(alpha, g_cfg.menu_open, 0.3f, 0);
}

float Menu::GetAlpha()
{
    return alpha * widget_alpha_mul;
}

float Menu::GetScale()
{
    return g_cfg.ui_scale / 100.0f;
}

void Menu::ApplyScale()
{
    static int applied_scale = 0;

    if (applied_scale == g_cfg.ui_scale)
    {
        return;
    }

    applied_scale = g_cfg.ui_scale;
    ImGui::GetStyle() = ImGuiStyle();
    Fonts::InitStyle();
    ImGui::GetStyle().ScaleAllSizes(GetScale());
    ImGui::GetStyle().MouseCursorScale = GetScale();
}

bool Menu::Vector_Getter(void* vec, int idx, const char** out_text)
{
    std::vector<std::string>& vector = *static_cast<std::vector<std::string>*>(vec);

    if (idx < 0 || idx >= (int)vector.size())
    {
        return false;
    }

    *out_text = vector.at(idx).c_str();

    return true;
}

bool Menu::Items_Array_Getter(void* data, int idx, const char** out_text)
{
    const char* const* items = (const char* const*)data;

    if (out_text)
    {
        *out_text = items[idx];
    }

    return true;
}

float Menu::Calc_Max_Popup_Height(int items_count)
{
    ImGuiContext& g = *GImGui;

    if (items_count <= 0)
    {
        return FLT_MAX;
    }

    const float k = g.IO.FontGlobalScale > 0.0f ? g.IO.FontGlobalScale : 1.0f;
    return (g.FontSize + g.Style.ItemSpacing.y * k) * items_count - g.Style.ItemSpacing.y * k + (g.Style.WindowPadding.y * 2.0f * k);
}

void Menu::Render_Vertical_Arrows(ImDrawList* list, ImVec2 pos, ImVec2 half_sz, float bar_w, float alpha)
{
    ImU32 alpha8 = IM_F32_TO_INT8_SAT(alpha);
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + half_sz.x + 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Right, IM_COL32(0, 0, 0, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + half_sz.x, pos.y), half_sz, ImGuiDir_Right, IM_COL32(255, 255, 255, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + bar_w - half_sz.x - 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Left, IM_COL32(0, 0, 0, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + bar_w - half_sz.x, pos.y), half_sz, ImGuiDir_Left, IM_COL32(255, 255, 255, alpha8));
}

void Menu::InitTextures()
{
    IDirect3DDevice9* device = Blur::GetDevice();

    if (!device)
    {
        return;
    }

    if (!logo_texture)
    {
        Texture::FromMemory(device, cheatLogo, sizeof(cheatLogo), &logo_texture);
    }

    if (!kb_texture)
    {
        Texture::FromMemory(device, keyboard_icon, sizeof(keyboard_icon), &kb_texture);
    }

    if (!icon_textures[0])
    {
        Texture::FromMemory(device, rage_icon, sizeof(rage_icon), &icon_textures[0]);
    }

    if (!icon_textures[1])
    {
        Texture::FromMemory(device, legit_icon, sizeof(legit_icon), &icon_textures[1]);
    }

    if (!icon_textures[2])
    {
        Texture::FromMemory(device, visuals_icon, sizeof(visuals_icon), &icon_textures[2]);
    }

    if (!icon_textures[3])
    {
        Texture::FromMemory(device, misc_icon, sizeof(misc_icon), &icon_textures[3]);
    }

    if (!icon_textures[4])
    {
        Texture::FromMemory(device, cfg_icon, sizeof(cfg_icon), &icon_textures[4]);
    }
}

void Menu::ReleaseTextures()
{
    if (logo_texture) { logo_texture->Release(); logo_texture = nullptr; }
    if (kb_texture) { kb_texture->Release(); kb_texture = nullptr; }
    for (auto& texture : icon_textures)
    {
        if (texture) { texture->Release(); texture = nullptr; }
    }
}

void Menu::WindowBegin()
{
    const float s = GetScale();
    // Keep a transparent interaction margin around the 838x535 Phobia-style panel.
    ImVec2 window_size = ImVec2(928.0f * s, 565.0f * s);
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);

    static int centered_scale = 0;
    static bool first_frame = true;

    if (first_frame || centered_scale != g_cfg.ui_scale)
    {
        first_frame = false;
        centered_scale = g_cfg.ui_scale;
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - window_size.x) * 0.5f, (ImGui::GetIO().DisplaySize.y - window_size.y) * 0.5f));
    }
    if (s_drag_pending)
    {
        ImGui::SetNextWindowPos(s_drag_pending_pos);
        s_drag_pending = false;
    }

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushFont(g_fonts.main);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    static bool keep_open = true;
    ImGui::Begin("##base_window", &keep_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PushItemWidth(256.0f * s);
    SetDrawList(ImGui::GetWindowDrawList());
    SetWindowPos(ImGui::GetWindowPos());

    ImDrawList* list = draw_list;
    list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    ImVec2 wp = GetWindowPos();
    list->PushClipRect(wp, ImVec2(wp.x + 838.0f * s, wp.y + 535.0f * s));
    ImGui::PushClipRect(wp, ImVec2(wp.x + 838.0f * s, wp.y + 535.0f * s), false);
}

void Menu::WindowEnd()
{
    ImDrawList* list = draw_list;
    list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);
    list->PopClipRect();
    ImGui::PopClipRect();
    ImGui::PopItemWidth();
    ImGui::End(false);

    ImGui::PopStyleColor();
    ImGui::PopFont();
    draw_list = nullptr;
}

void Menu::DrawBackground()
{
    const float s = GetScale();
    ImDrawList* list = draw_list;
    const float a = GetAlpha();
    const int alpha8 = (int)(255.0f * a);
    const ImVec2 pos = GetWindowPos();
    const ImVec2 size(838.0f * s, 535.0f * s);
    const float side = 160.0f * s;
    const c_color accent = g_cfg.accent.to_color();

    // Phobia-inspired shell: charcoal navigation rail and a darker workspace.
    list->AddRectFilled(pos + ImVec2(4.0f * s, 5.0f * s), pos + size + ImVec2(7.0f * s, 8.0f * s),
        IM_COL32(0, 0, 0, (int)(95.0f * a)), 12.0f * s);
    list->AddRectFilled(pos, pos + ImVec2(side, size.y), IM_COL32(32, 32, 32, alpha8),
        10.0f * s, ImDrawCornerFlags_Left);
    list->AddRectFilled(pos + ImVec2(side - 1.0f * s, 0), pos + size, IM_COL32(26, 26, 26, alpha8),
        10.0f * s, ImDrawCornerFlags_Right);
    list->AddLine(pos + ImVec2(side, 0), pos + ImVec2(side, size.y), IM_COL32(50, 50, 50, alpha8));
    list->AddRect(pos, pos + size, IM_COL32(50, 50, 50, alpha8), 10.0f * s, 0, 1.0f * s);

    // Compact text logo, matching menu #8's logo block without external assets.
    list->AddRectFilled(pos + ImVec2(16.0f * s, 18.0f * s), pos + ImVec2(45.0f * s, 47.0f * s),
        accent.new_alpha(alpha8).as_imcolor(), 7.0f * s);
    list->AddText(g_fonts.dmg, 18.0f * s, pos + ImVec2(22.0f * s, 23.0f * s),
        IM_COL32(255, 255, 255, alpha8), "18");
    list->AddText(g_fonts.dmg, 18.0f * s, pos + ImVec2(53.0f * s, 17.0f * s),
        IM_COL32(245, 245, 245, alpha8), "18:32");
    list->AddText(pos + ImVec2(54.0f * s, 37.0f * s), IM_COL32(105, 105, 105, alpha8), "CHEAT");
    list->AddLine(pos + ImVec2(12.0f * s, 61.0f * s), pos + ImVec2(148.0f * s, 61.0f * s),
        IM_COL32(50, 50, 50, alpha8));

    // User/status card at the bottom of the navigation rail.
    const ImVec2 cardMin = pos + ImVec2(9.0f * s, 487.0f * s);
    const ImVec2 cardMax = pos + ImVec2(151.0f * s, 525.0f * s);
    list->AddRectFilled(cardMin, cardMax, IM_COL32(41, 41, 41, alpha8), 5.0f * s);
    list->AddRect(cardMin, cardMax, IM_COL32(50, 50, 50, alpha8), 5.0f * s);
    list->AddCircleFilled(cardMin + ImVec2(19.0f * s, 19.0f * s), 11.0f * s,
        accent.new_alpha(alpha8).as_imcolor());
    list->AddText(cardMin + ImVec2(38.0f * s, 6.0f * s), IM_COL32(235, 235, 235, alpha8), "18:32 user");
    list->AddText(cardMin + ImVec2(38.0f * s, 21.0f * s), IM_COL32(105, 105, 105, alpha8), "lifetime");

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 dragMax = pos + ImVec2(838.0f * s, 66.0f * s);
    if (!s_dragging && ImGui::IsMouseHoveringRect(pos, dragMax) && ImGui::IsMouseClicked(0))
    {
        s_dragging = true;
        s_drag_offset = ImVec2(io.MousePos.x - pos.x, io.MousePos.y - pos.y);
    }

    s_drag_pending = false;
    if (s_dragging)
    {
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
        {
            s_drag_pending = true;
            s_drag_pending_pos = ImVec2(io.MousePos.x - s_drag_offset.x - 45.0f * s,
                io.MousePos.y - s_drag_offset.y - 15.0f * s);
        }
        else
        {
            s_dragging = false;
        }
    }
}

void Menu::DrawTabs()
{
    const float s = GetScale();
    const float a = GetAlpha();
    ImDrawList* list = draw_list;
    const ImVec2 savedCursor = ImGui::GetCursorPos();
    const ImVec2 base = GetWindowPos();
    const c_color accent = g_cfg.accent.to_color();
    const std::string names[5] = {
        tr("Рейдж", "Rage"), tr("Легит", "Legit"), tr("Визуалы", "Visuals"),
        tr("Разное", "Misc"), tr("Профиль", "Profile")
    };
    // Keep the original five menu categories and their familiar order.
    const float rows[5] = { 78.0f, 116.0f, 154.0f, 192.0f, 230.0f };

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));

    for (int i = 0; i < 5; ++i)
    {
        tab_animation_t& info = tab_info[i];
        ImGui::SetCursorPos(ImVec2(53.0f * s, (15.0f + rows[i]) * s));
        const std::string id = "##phobia_tab_" + std::to_string(i);
        const bool clicked = ImGui::ButtonEx(id.c_str(), ImVec2(144.0f * s, 32.0f * s), 0, &info.hovered);
        if (clicked) tab_selector = i;

        CreateAnimation(info.hovered_alpha, info.hovered, 1.0f, AnimLerp);
        CreateAnimation(info.alpha, tab_selector == i, 0.8f, AnimSkipDisable | AnimLerp);

        const ImVec2 tmin = base + ImVec2(8.0f * s, rows[i] * s);
        const ImVec2 tmax = tmin + ImVec2(144.0f * s, 32.0f * s);
        if (info.hovered_alpha > 0.01f || info.alpha > 0.01f)
        {
            const int bg = (int)((18.0f * info.hovered_alpha + 20.0f * info.alpha) * a);
            list->AddRectFilled(tmin, tmax, IM_COL32(70, 70, 70, bg), 5.0f * s);
        }
        if (tab_selector == i)
        {
            list->AddRectFilled(tmin, tmin + ImVec2(3.0f * s, 32.0f * s),
                accent.new_alpha((int)(255 * info.alpha * a)).as_imcolor(), 2.0f * s);
        }

        const int idle = (int)((145.0f + 110.0f * info.hovered_alpha) * a);
        const ImU32 tint = tab_selector == i
            ? IM_COL32(245, 245, 245, (int)(255 * a))
            : IM_COL32(idle, idle, idle, (int)(255 * a));
        if (i < 4 && icon_textures[i])
        {
            list->AddImage((void*)icon_textures[i], tmin + ImVec2(11.0f * s, 9.0f * s),
                tmin + ImVec2(26.0f * s, 24.0f * s), ImVec2(0, 0), ImVec2(1, 1), tint);
        }
        else
        {
            list->AddCircleFilled(tmin + ImVec2(18.0f * s, 16.0f * s), 5.0f * s, tint);
        }
        list->AddText(tmin + ImVec2(35.0f * s, 8.0f * s), tint, names[i].c_str());
    }

    ImGui::PopStyleColor(3);
    ImGui::SetCursorPos(savedCursor);
}

void Menu::DrawSubTabs(int& selector, const std::vector<std::string>& tabs)
{
    const float s = GetScale();
    ImGuiStyle& style = ImGui::GetStyle();
    float a = GetAlpha();
    float wa = 255.0f * a;
    ImVec2 child_pos = GetWindowPos() + ImVec2(178.0f * s, 62.0f * s);
    ImVec2 prev = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, a));
    draw_list->AddRectFilled(child_pos, child_pos + ImVec2(646.0f * s, 58.0f * s), ImColor(32, 32, 32, (int)(235 * a)), 5.0f * s);
    draw_list->AddRect(child_pos, child_pos + ImVec2(646.0f * s, 58.0f * s), ImColor(50, 50, 50, (int)(255 * a)), 5.0f * s);

    c_color accent = g_cfg.accent.to_color();

    for (int i = 0; i < (int)tabs.size(); i++)
    {
        tab_animation_t& info = subtab_info[tabs[0]][i];
        ImVec2 cur = ImVec2((27.0f + 80.0f * i) * s, 14.0f * s);
        ImGui::SetCursorPos(cur);
        ImVec2 tsize = ImVec2(70.0f * s, 32.0f * s);

        std::string id = std::string("##sub_") + tabs[i];
        bool clicked = ImGui::ButtonEx(id.c_str(), tsize, 0, &info.hovered);

        if (clicked)
        {
            selector = i;
        }

        CreateAnimation(info.hovered_alpha, info.hovered, 1.0f, AnimLerp);
        CreateAnimation(info.alpha, selector == i, 0.8f, AnimSkipDisable | AnimLerp);

        ImVec2 tmin = child_pos + cur - ImVec2(8.0f * s, 0);
        ImVec2 tmax = tmin + tsize;
        ImRect bb = ImRect(tmin, tmax);

        if (selector == i)
        {
            draw_list->AddRectFilled(bb.Min, bb.Max, ImColor(41, 41, 41, (int)(255 * a * info.alpha)), 4.0f * s);
            draw_list->PushClipRect(ImVec2(bb.Min.x + 15.0f * s, bb.Max.y - 2.0f * s), ImVec2(bb.Max.x - 15.0f * s, bb.Max.y));
            draw_list->AddRectFilled(ImVec2(bb.Min.x + 15.0f * s, bb.Max.y - 2.0f * s), ImVec2(bb.Max.x - 15.0f * s, bb.Max.y + 4.0f * s), accent.new_alpha((int)(info.alpha * wa)).as_imcolor(), 2.0f * s, ImDrawCornerFlags_Top);
            draw_list->PopClipRect();
        }

        float rgb = selector == i ? 255 : 150 + 105 * info.hovered_alpha;
        c_color tc = c_color((int)rgb, (int)rgb, (int)rgb, (int)(rgb * a));
        ImVec2 ls = ImGui::CalcTextSize(tabs[i].c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(tc.r() / 255.0f, tc.g() / 255.0f, tc.b() / 255.0f, tc.a() / 255.0f));
        ImGui::RenderTextClipped(bb.Min, bb.Max, tabs[i].c_str(), NULL, &ls, style.ButtonTextAlign, &bb);
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor(4);
    ImGui::SetCursorPos(prev);
    ImGui::ItemSize(ImVec2(0, 62.0f * s));
    ImGui::PushClipRect(child_pos + ImVec2(0.0f, 62.0f * s), child_pos + ImVec2(658.0f * s, 472.0f * s), false);
    draw_list->PushClipRect(child_pos + ImVec2(0.0f, 62.0f * s), child_pos + ImVec2(658.0f * s, 472.0f * s));
}

void Menu::UpdateSubFade(int tab, int& sub)
{
    if (sub != prev_sub[tab])
    {
        for (auto& a : item_animations)
        {
            a.second.reset();
        }

        subtab_alpha[tab] = 0.0f;
        prev_sub[tab] = sub;
    }

    CreateAnimation(subtab_alpha[tab], g_cfg.menu_open && sub == prev_sub[tab], 0.2f, AnimSkipDisable | AnimLerp);
    alpha = subtab_alpha[tab] * tab_alpha;
}

void Menu::GroupBegin(const char* label)
{
    const float s = GetScale();
    const float a = GetAlpha();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const c_color accent = g_cfg.accent.to_color();
    draw_list->AddRectFilled(start + ImVec2(0, 2.0f * s), start + ImVec2(3.0f * s, 15.0f * s),
        accent.new_alpha((int)(255 * a)).as_imcolor(), 1.5f * s);
    ImGui::SetCursorScreenPos(start + ImVec2(10.0f * s, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.92f, 0.92f, a));
    ImGui::Text(label);
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(start.x, ImGui::GetCursorScreenPos().y));
    ImGui::ItemSize(ImVec2(0, 3.0f * s));
    ImGui::BeginGroup();
}

void Menu::GroupEnd()
{
    ImGui::EndGroup();
    ImGui::ItemSize(ImVec2(0, 9.0f * GetScale()));
}

void Menu::DrawContent()
{
    ImVec2 window_pos = GetWindowPos();
    ImVec2 prev_pos = ImGui::GetCursorPos();
    float old_alpha = GetAlpha();

    if (tab_selector != prev_tab)
    {
        for (auto& a : item_animations)
        {
            a.second.reset();
        }

        tab_alpha = 0.0f;
        prev_tab = tab_selector;
    }

    CreateAnimation(tab_alpha, g_cfg.menu_open && tab_selector == prev_tab, 0.2f, AnimSkipDisable | AnimLerp);
    alpha = tab_alpha;

    const float s = GetScale();
    ImGui::SetCursorPos(ImVec2(215.0f * s, 77.0f * s));
    ImRect window_bb = ImRect(window_pos + ImVec2(160.0f * s, 47.0f * s), window_pos + ImVec2(838.0f * s, 535.0f * s));
    ImGui::PushClipRect(window_bb.Min, window_bb.Max, false);
    ImGui::BeginChild("##tab_child", ImVec2(), true);

    if (tab_selector == 0)
    {
        DrawRage();
    }
    else if (tab_selector == 1)
    {
        DrawLegit();
    }

    else if (tab_selector == 2)
    {
        DrawVisuals();
    }

    else if (tab_selector == 3)
    {
        DrawMisc();
    }

    else if (tab_selector == 4)
    {
        DrawProfile();
    }

    ImGui::EndChild(false);
    alpha = old_alpha;
    ImGui::PopClipRect();
    ImGui::SetCursorPos(prev_pos);
}

void Menu::Draw()
{
    InitTextures();
    UpdateAlpha();

    if (!g_cfg.menu_open && alpha <= 0.01f)
    {
        return;
    }

    ApplyScale();
    ImGui::SetColorEditOptions(ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_DisplayRGB);
    WindowBegin();
    DrawBackground();
    DrawTabs();
    DrawContent();
    draw_list->PopClipRect();
    ImGui::PopClipRect();
    WindowEnd();
}




