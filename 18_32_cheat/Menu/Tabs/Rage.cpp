#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawRage()
{
    static float collisionFade = 0.0f;
    static float rapidFade = 0.0f;
    static float randomGodFade = 0.0f;

    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("subtab_rage", ImVec2(), false);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    GroupBegin(tr("Оружие", "Weapon"));
    BindableCheckbox("rapidfire", tr("Новый рапид", "New rapid"),
        &g_cfg.rapidfire, &g_cfg.rapidfire_bind);
    CreateAnimation(rapidFade, g_cfg.rapidfire, 0.3f, AnimLerp);
    if (g_cfg.rapidfire || rapidFade > 0.02f)
    {
        const float previousAlpha = widget_alpha_mul;
        widget_alpha_mul = rapidFade;
        if (SliderFloat(tr("Множитель", "Multiplier"),
            &g_cfg.rapidfire_multiplier, 1.0f, 10.0f, "%.1f"))
        {
            g_cfg.rapidfire_multiplier = std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f;
            SaveGeneralConfig();
        }
        widget_alpha_mul = previousAlpha;
    }
    BindableCheckbox("airbreak", tr("Аир-брейк", "Air brake"),
        &g_cfg.airbreake, &g_cfg.airbreake_bind);
    GroupEnd();

    ImGui::NextColumn();
    GroupBegin(tr("Игрок", "Player"));
    BindableCheckbox("godmode", tr("Годмод", "God mode"),
        &g_cfg.godmode, &g_cfg.godmode_bind);
    BindableCheckbox("random_godmode", tr("Рандом ГМ", "Random god mode"),
        &g_cfg.randomgodmode, &g_cfg.randomgodmode_bind);
    CreateAnimation(randomGodFade, g_cfg.randomgodmode, 0.3f, AnimLerp);
    if (g_cfg.randomgodmode || randomGodFade > 0.02f)
    {
        const float previousAlpha = widget_alpha_mul;
        widget_alpha_mul = randomGodFade;
        if (SliderInt(tr("Шанс", "Chance"), &g_cfg.randomgodmode_chance, 1, 100, "%d%%"))
        {
            SaveGeneralConfig();
        }
        widget_alpha_mul = previousAlpha;
    }
    BindableCheckbox("double_jump", tr("Двойной прыжок", "Double jump"),
        &g_cfg.doublejump, &g_cfg.doublejump_bind);
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
