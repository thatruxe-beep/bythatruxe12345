#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawRage()
{
    static float collisionFade = 0.0f;

    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("subtab_rage", ImVec2(), false);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    GroupBegin(tr("Оружие", "Weapon"));
    BindableCheckbox("rapidfire", tr("Рапид", "Rapid fire"),
        &g_cfg.rapidfire, &g_cfg.rapidfire_bind);
    BindableCheckbox("airbreak", tr("Аир-брейк", "Air brake"),
        &g_cfg.airbreake, &g_cfg.airbreake_bind);
    GroupEnd();

    ImGui::NextColumn();
    GroupBegin(tr("Игрок", "Player"));
    BindableCheckbox("godmode", tr("Годмод", "God mode"),
        &g_cfg.godmode, &g_cfg.godmode_bind);
    BindableCheckbox("nocol", tr("Анти-коллизия", "Anti collision"),
        &g_cfg.nocol, &g_cfg.nocol_bind);

    CreateAnimation(collisionFade, g_cfg.nocol, 0.3f, AnimLerp);
    if (g_cfg.nocol || collisionFade > 0.02f)
    {
        const float previousAlpha = widget_alpha_mul;
        widget_alpha_mul = collisionFade;
        std::vector<std::string> targets = {
            tr("Машины", "Vehicles"),
            tr("Педы", "Peds"),
            tr("Объекты", "Objects")
        };
        const unsigned int previousFlags = g_cfg.nocol_flags;
        MultiCombo(tr("Игнорировать", "Ignore"), g_cfg.nocol_flags, targets);
        if (previousFlags != g_cfg.nocol_flags)
        {
            SaveGeneralConfig();
        }
        widget_alpha_mul = previousAlpha;
    }
    GroupEnd();

    ImGui::EndChild(false);
}
