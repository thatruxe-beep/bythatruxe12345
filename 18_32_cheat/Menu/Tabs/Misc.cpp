#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawMisc()
{
    static int subtab = 0;
    static float runFade = 0.0f;
    static float speedFade = 0.0f;
    static float aspectFade = 0.0f;

    std::vector<std::string> items = {
        tr("Игрок", "Player"),
        tr("Движение", "Movement"),
        tr("Машина", "Vehicle")
    };
    DrawSubTabs(subtab, items);
    UpdateSubFade(3, subtab);
    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("subtab_misc", ImVec2(), false);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    if (subtab == 0)
    {
        GroupBegin(tr("Игрок", "Player"));
        BindableCheckbox("nofall", tr("Без урона от падения", "No fall damage"),
            &g_cfg.nofall, &g_cfg.nofall_bind);
        if (Button(tr("Восстановить здоровье", "Heal HP")))
        {
            Heal::Update();
        }
        GroupEnd();
    }
    else if (subtab == 1)
    {
        GroupBegin(tr("Движение", "Movement"));
        BindableCheckbox("fast_run", tr("Быстрый бег", "Fast run"),
            &g_cfg.fastbeg, &g_cfg.fastbeg_bind);
        CreateAnimation(runFade, g_cfg.fastbeg, 0.3f, AnimLerp);
        if (g_cfg.fastbeg || runFade > 0.02f)
        {
            const float previousAlpha = widget_alpha_mul;
            widget_alpha_mul = runFade;
            if (SliderFloat(tr("Скорость", "Speed"), &g_cfg.fastbegs, 1.0f, 10.0f))
            {
                SaveGeneralConfig();
            }
            widget_alpha_mul = previousAlpha;
        }
        BindableCheckbox("fastrot", tr("Быстрый поворот", "Fast rotation"),
            &g_cfg.fastrot, &g_cfg.fastrot_bind);
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
    else
    {
        GroupBegin(tr("Машина", "Vehicle"));
        if (Button(tr("Починить", "Repair vehicle")))
        {
            Repair::Update();
        }
        if (Button(tr("Перевернуть", "Flip")))
        {
            Flip::Update();
        }
        BindableCheckbox("speedhack", tr("Спидхак", "Speedhack"),
            &g_cfg.speedhack, &g_cfg.speedhack_bind);
        CreateAnimation(speedFade, g_cfg.speedhack, 0.3f, AnimLerp);
        if (g_cfg.speedhack || speedFade > 0.02f)
        {
            const float previousAlpha = widget_alpha_mul;
            widget_alpha_mul = speedFade;
            if (SliderFloat(tr("Мощность", "Power"), &g_cfg.MaxSpd,
                0.0f, 30.0f, "%.1f"))
            {
                SaveGeneralConfig();
            }
            widget_alpha_mul = previousAlpha;
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Автоматизация", "Automation"));
        BindableCheckbox("autoengine", tr("Автозапуск двигателя", "Auto engine"),
            &g_cfg.autoengine, &g_cfg.autoengine_bind);
        BindableCheckbox("autounlock", tr("Авторазблокировка", "Auto unlock"),
            &g_cfg.autounlock, &g_cfg.autounlock_bind);
        BindableCheckbox("nobikefall", tr("Не падать с байка", "No bike fall"),
            &g_cfg.nobikefall, &g_cfg.nobikefall_bind);
        GroupEnd();
    }

    ImGui::EndChild(false);
}
