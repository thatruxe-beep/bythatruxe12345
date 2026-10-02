#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawLegit()
{
    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("subtab_legit", ImVec2(), false);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    GroupBegin(tr("Точность", "Accuracy"));
    BindableCheckbox("nospread", tr("Без разброса", "No spread"),
        &g_cfg.nospread, &g_cfg.nospread_bind);
    BindableCheckbox("fastcross", tr("Быстрый прицел", "Fast crosshair"),
        &g_cfg.fastcross, &g_cfg.fastcross_bind);
    if (Checkbox(tr("Быстрый зум (ПКМ + СКМ)", "Fast zoom (RMB + MMB)"),
        &g_cfg.fastzoom))
    {
        SaveGeneralConfig();
    }
    GroupEnd();

    ImGui::EndChild(false);
}
