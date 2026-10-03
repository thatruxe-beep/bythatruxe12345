#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawVisuals()
{
    static int subtab = 0;
    static float wallhackFade = 0.0f;
    static float timeFade = 0.0f;
    static float aspectFade = 0.0f;

    std::vector<std::string> tabs = {
        tr("Валлхак", "Wallhack"),
        tr("Эффекты", "Effects")
    };
    DrawSubTabs(subtab, tabs);
    UpdateSubFade(2, subtab);
    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("subtab_visual", ImVec2(), false);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    if (subtab == 0)
    {
        GroupBegin(tr("Валлхак", "Wallhack"));
        BindableCheckbox("wallhack", tr("Включить", "Enable"),
            &g_cfg.wh, &g_cfg.wh_bind);
        CreateAnimation(wallhackFade, g_cfg.wh, 0.3f, AnimLerp);
        if (g_cfg.wh || wallhackFade > 0.02f)
        {
            const float previousAlpha = widget_alpha_mul;
            widget_alpha_mul = wallhackFade;
            if (SliderFloat(tr("Дальность", "Range"), &g_cfg.whDistance,
                10.0f, 1000.0f))
            {
                SaveGeneralConfig();
            }
            widget_alpha_mul = previousAlpha;
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin("ESP");
        const float previousAlpha = widget_alpha_mul;
        widget_alpha_mul = wallhackFade;

        auto flagCheckbox = [&](const char* label, unsigned int flag)
        {
            bool enabled = (g_cfg.wh_flags & flag) != 0;
            if (Checkbox(label, &enabled))
            {
                if (enabled) g_cfg.wh_flags |= flag;
                else g_cfg.wh_flags &= ~flag;
                SaveGeneralConfig();
            }
        };

        flagCheckbox(tr("2D бокс", "2D box"), WH_BOX);
        flagCheckbox(tr("Полоса HP", "HP bar"), WH_HP);
        flagCheckbox(tr("Полоса брони", "Armor bar"), WH_ARMOR);
        flagCheckbox(tr("HP / броня текстом", "HP / armor text"), WH_TEXT);
        flagCheckbox(tr("Дистанция", "Distance"), WH_DIST);
        flagCheckbox(tr("Оружие", "Weapon"), WH_WEAPON);
        flagCheckbox(tr("Скелет", "Skeleton"), WH_SKELETON);
        flagCheckbox(tr("Трассер снизу", "Bottom tracer"), WH_SNAP);

        bool colorChanged = false;
        if (g_cfg.wh_flags & WH_BOX) colorChanged |= ColorPicker(tr("Цвет бокса", "Box color"), g_cfg.whcol);
        if (g_cfg.wh_flags & WH_HP) colorChanged |= ColorPicker(tr("Цвет HP", "HP color"), g_cfg.hpcol);
        if (g_cfg.wh_flags & WH_ARMOR) colorChanged |= ColorPicker(tr("Цвет брони", "Armor color"), g_cfg.armorcol);
        if (g_cfg.wh_flags & (WH_TEXT | WH_DIST)) colorChanged |= ColorPicker(tr("Цвет текста", "Text color"), g_cfg.distcol);
        if (g_cfg.wh_flags & WH_SKELETON) colorChanged |= ColorPicker(tr("Цвет скелета", "Skeleton color"), g_cfg.skelcol);
        if (g_cfg.wh_flags & WH_SNAP) colorChanged |= ColorPicker(tr("Цвет трассера", "Tracer color"), g_cfg.snapcol);
        if (colorChanged) SaveGeneralConfig();

        widget_alpha_mul = previousAlpha;
        GroupEnd();
    }
    else
    {
        GroupBegin(tr("Эффекты", "Effects"));
        BindableCheckbox("nightmode", tr("Ночной режим", "Night mode"),
            &g_cfg.nightmode, &g_cfg.nightmode_bind);
        BindableCheckbox("customtime", tr("Своё время", "Custom time"),
            &g_cfg.customtime, &g_cfg.customtime_bind);
        CreateAnimation(timeFade, g_cfg.customtime, 0.3f, AnimLerp);
        if (g_cfg.customtime || timeFade > 0.02f)
        {
            const float previousAlpha = widget_alpha_mul;
            widget_alpha_mul = timeFade;
            if (SliderFloat(tr("Час", "Hour"), &g_cfg.timehour,
                0.0f, 23.0f, "%.0f"))
            {
                SaveGeneralConfig();
            }
            widget_alpha_mul = previousAlpha;
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Экран", "Display"));
        BindableCheckbox("aspect", tr("Соотношение сторон", "Aspect ratio"),
            &g_cfg.aspect, &g_cfg.aspect_bind);
        CreateAnimation(aspectFade, g_cfg.aspect, 0.3f, AnimLerp);
        if (g_cfg.aspect || aspectFade > 0.02f)
        {
            const float previousAlpha = widget_alpha_mul;
            widget_alpha_mul = aspectFade;
            if (SliderFloat(tr("Значение", "Ratio"), &g_cfg.aspectval,
                0.5f, 3.5f, "%.2f"))
            {
                SaveGeneralConfig();
            }
            widget_alpha_mul = previousAlpha;
        }
        GroupEnd();
    }

    ImGui::EndChild(false);
}
