#include "Menu/Menu.hpp"

#include "Game/Features.h"
#include "Hooks/d3d9/Present.hpp"

#include "Gfx/Fonts.hpp"

#include <shellapi.h>

#include <cmath>

namespace
{
    // Фирменный циферблат 18:32 (общий для профиля и окна биндов).
    void DrawClock(ImDrawList* list, const ImVec2& center, float radius,
        ImU32 accent, ImU32 dim, float thickness)
    {
        list->AddCircle(center, radius, dim, 40, thickness);

        const float hourAngle = (18.0f + 32.0f / 60.0f) / 12.0f * 2.0f * IM_PI;
        const float minuteAngle = 32.0f / 60.0f * 2.0f * IM_PI;

        const ImVec2 hourEnd(
            center.x + std::sin(hourAngle) * radius * 0.52f,
            center.y - std::cos(hourAngle) * radius * 0.52f);
        const ImVec2 minuteEnd(
            center.x + std::sin(minuteAngle) * radius * 0.82f,
            center.y - std::cos(minuteAngle) * radius * 0.82f);

        list->AddLine(center, hourEnd, accent, thickness);
        list->AddLine(center, minuteEnd, accent, thickness);
        list->AddCircleFilled(center, radius * 0.08f, accent, 8);
    }

    // Строка "ключ — значение" в карточке.
    void InfoRow(ImDrawList* list, const ImVec2& rowMin, float width, float s,
        float a, const ImU32& accent, const char* key, const std::string& value)
    {
        list->AddCircleFilled(ImVec2(rowMin.x + 2.0f * s, rowMin.y + ImGui::GetTextLineHeight() * 0.55f),
            1.6f * s, accent, 8);
        list->AddText(ImVec2(rowMin.x + 10.0f * s, rowMin.y),
            IM_COL32(255, 255, 255, (int)(130 * a)), key);
        const ImVec2 valueSize = ImGui::CalcTextSize(value.c_str());
        list->AddText(ImVec2(rowMin.x + width - valueSize.x, rowMin.y),
            IM_COL32(240, 240, 244, (int)(235 * a)), value.c_str());
    }
}

void Menu::DrawProfile()
{
        const float s = GetScale();
        static int sub4 = 0;
        static bool cfg_scanned = false;

        if (!cfg_scanned)
        {
            config_t::RefreshList();
            cfg_scanned = true;
        }

        std::vector<std::string> items = { tr("Конфиг", "Config"), tr("Настройки", "Settings"), tr("Важное", "Important") };
        DrawSubTabs(sub4, items);
        UpdateSubFade(4, sub4);
        ImGui::SetCursorPosX(0);
        ImGui::BeginChild("subtab_cfg", ImVec2(), false);

        if (sub4 == 0)
        {
            const bool wrong_config = g_cfg.cfg_list.empty() || g_cfg.cfg_selected < 0 || g_cfg.cfg_selected >= (int)g_cfg.cfg_list.size();
            const char* selected_cfg = wrong_config ? nullptr : g_cfg.cfg_list[g_cfg.cfg_selected].c_str();

            ImGui::Columns(2, NULL, false);
            ImGui::SetColumnOffset(1, 270.0f * GetScale());

            // Слева — весь список: поиск сверху, ниже сами конфиги.
            std::string list_title = tr("Конфигурации", "Configurations");
            GroupBegin(list_title.c_str());
            Textbox(tr("Поиск", "Search"), g_cfg.cfg_search, sizeof(g_cfg.cfg_search));
            Listbox("configlist___", &g_cfg.cfg_selected, g_cfg.cfg_list, 12, g_cfg.cfg_search);
            GroupEnd();

            ImGui::NextColumn();

            // Справа — карточка выбранного конфига и действия над ним.
            std::string card_title = std::string(tr("Выбран", "Selected")) + ": "
                + (wrong_config ? tr("нет", "none") : selected_cfg);
            GroupBegin(card_title.c_str());
            if (Button(tr("Загрузить", "Load")) && !wrong_config)
            {
                g_cfg.Load(selected_cfg);
            }
            if (Button(tr("Сохранить", "Save")) && !wrong_config)
            {
                g_cfg.Save(selected_cfg);
            }
            if (Button(tr("Обновить список", "Refresh list")))
            {
                config_t::RefreshList();
            }
            if (Button(tr("Открыть папку", "Open folder")))
            {
                const std::string folder = config_t::ConfigDir();
                CreateDirectoryA(folder.c_str(), nullptr);

                if ((INT_PTR)ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL) <= 32)
                {
                    MessageBoxA(NULL, tr("Не удалось открыть папку конфигов", "Failed to open configs folder"), "18:32 cheat", MB_OK | MB_ICONERROR);
                }
            }
            GroupEnd();

            GroupBegin(tr("Новый конфиг", "New config"));
            Textbox(tr("Имя", "Name"), g_cfg.cfg_name, sizeof(g_cfg.cfg_name));
            if (Button(tr("Создать", "Create")))
            {
                if (g_cfg.cfg_name[0] != 0)
                {
                    bool exists = false;

                    for (const auto& name : g_cfg.cfg_list)
                    {
                        if (name == g_cfg.cfg_name)
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists)
                    {
                        g_cfg.cfg_list.emplace_back(g_cfg.cfg_name);
                        g_cfg.cfg_selected = (int)g_cfg.cfg_list.size() - 1;
                        g_cfg.Save(g_cfg.cfg_name);
                        g_cfg.cfg_name[0] = 0;
                    }
                }
            }
            GroupEnd();

            GroupBegin(tr("Опасная зона", "Danger zone"));
            if (Button(tr("Сбросить (значения по умолчанию)", "Reset (default values)")) && !wrong_config)
            {
                std::string target = selected_cfg;
                const bool was_open = g_cfg.menu_open;
                std::vector<std::string> saved_list = g_cfg.cfg_list;
                const int saved_selected = g_cfg.cfg_selected;
                g_cfg = config_t();
                g_cfg.menu_open = was_open;
                g_cfg.cfg_list = saved_list;
                g_cfg.cfg_selected = saved_selected;
                g_cfg.Save(target);
            }
            if (Button(tr("Удалить конфиг", "Delete config")) && !wrong_config)
            {
                g_cfg.Remove(selected_cfg);
            }
            GroupEnd();

            ImGui::Columns(1);
        }
        else if (sub4 == 1)
        {
            ImGui::Columns(2, NULL, false);
            ImGui::SetColumnOffset(1, 270.0f * GetScale());

            GroupBegin(tr("Оформление", "Appearance"));
            if (ColorPicker(tr("Акцент интерфейса", "Interface accent"), g_cfg.accent))
            {
                SaveGeneralConfig();
            }

            static const std::string dpi_s[] = { "75%", "100%", "125%", "150%", "175%", "200%" };
            const char* dpi_items[] = { dpi_s[0].c_str(), dpi_s[1].c_str(), dpi_s[2].c_str(), dpi_s[3].c_str(), dpi_s[4].c_str(), dpi_s[5].c_str() };
            const int dpi_values[] = { 75, 100, 125, 150, 175, 200 };
            int dpi_selected = 1;

            for (int i = 0; i < 6; i++)
            {
                if (dpi_values[i] == g_cfg.ui_scale)
                {
                    dpi_selected = i;
                    break;
                }
            }

            if (Combo(tr("Масштаб DPI", "DPI scale"), &dpi_selected, dpi_items, 6))
            {
                g_cfg.ui_scale = dpi_values[dpi_selected];
                SaveGeneralConfig();
            }
            GroupEnd();

            ImGui::NextColumn();
            GroupBegin(tr("Меню и язык", "Menu and language"));
            if (Checkbox(tr("Окно активных биндов", "Active binds window"), &g_cfg.showbinds))
            {
                SaveGeneralConfig();
            }

            static const std::string lang_s[] = { "Русский", "English" };
            const char* lang_items[] = { lang_s[0].c_str(), lang_s[1].c_str() };
            if (Combo(tr("Язык", "Language"), &g_cfg.language, lang_items, 2))
            {
                SaveGeneralConfig();
            }
            GroupEnd();

            ImGui::Columns(1);
        }
        else
        {
            // Карточка "О программе": циферблат, название, строки информации.
            const float a = GetAlpha();
            const float ccx = (ImGui::GetWindowContentRegionMin().x + ImGui::GetWindowContentRegionMax().x) * 0.5f;
            const float width = ImGui::GetWindowContentRegionWidth();
            const c_color accent = g_cfg.accent.to_color();
            const ImU32 accentU32 = static_cast<ImU32>(accent.new_alpha((int)(255 * a)).as_imcolor());

            ImGui::Dummy(ImVec2(0, 14.0f * s));

            const ImVec2 clockCenter(ccx, ImGui::GetCursorScreenPos().y + 20.0f * s);
            DrawClock(draw_list, clockCenter, 20.0f * s, accentU32,
                IM_COL32(255, 255, 255, (int)(140 * a)), 2.2f * s);

            ImGui::Dummy(ImVec2(0, 44.0f * s));

            static const std::string dev_s = "18:32 cheat";
            ImGui::PushFont(g_fonts.dmg);
            ImVec2 nick_size = ImGui::CalcTextSize(dev_s.c_str());
            ImGui::SetCursorPosX(ccx - nick_size.x * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, a));
            ImGui::Text(dev_s.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();

            static const std::string role_s = "Разработчик";
            static const std::string role_e = "Developer";
            const char* role = (g_cfg.language == 1 ? role_e : role_s).c_str();
            ImVec2 role_size = ImGui::CalcTextSize(role);
            ImGui::SetCursorPosX(ccx - role_size.x * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.5f * a));
            ImGui::Text(role);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 14.0f * s));

            const float card_pad = 16.0f * s;
            const float row_h = ImGui::GetTextLineHeight() + 8.0f * s;
            const int rows = 2;
            ImVec2 card_min(ImGui::GetCursorScreenPos().x + 90.0f * s, ImGui::GetCursorScreenPos().y);
            ImVec2 card_max(card_min.x + width - 180.0f * s, card_min.y + row_h * rows + card_pad);
            draw_list->AddRectFilled(card_min, card_max, IM_COL32(20, 21, 26, (int)(190 * a)), 4.0f * s);
            draw_list->AddRect(card_min, card_max, IM_COL32(255, 255, 255, (int)(22 * a)), 4.0f * s);
            draw_list->AddRectFilled(card_min, ImVec2(card_min.x + 3.0f * s, card_max.y), accentU32);

            InfoRow(draw_list, ImVec2(card_min.x + card_pad, card_min.y + card_pad * 0.75f),
                card_max.x - card_min.x - card_pad * 2.0f, s, a, accentU32,
                tr("Контакты", "Contacts"), "t.me/thatruxe");
            InfoRow(draw_list, ImVec2(card_min.x + card_pad, card_min.y + card_pad * 0.75f + row_h),
                card_max.x - card_min.x - card_pad * 2.0f, s, a, accentU32,
                tr("Сборка", "Build"), "18:32");

            ImGui::SetCursorScreenPos(card_max);
            ImGui::Dummy(ImVec2(0, 16.0f * s));
            ImGui::SetCursorPosX(ccx - 128.0f * s);

            if (Button(tr("Написать разработчику", "Contact developer")))
            {
                ShellExecuteA(NULL, "open", "https://t.me/thatruxe", NULL, NULL, SW_SHOWNORMAL);
            }

            ImGui::Dummy(ImVec2(0, 8.0f * s));

            // Выгрузка: красная волосяная линия сверху как предупреждение.
            ImVec2 line_min = ImGui::GetCursorScreenPos();
            draw_list->AddLine(ImVec2(line_min.x + 128.0f * s - 100.0f * s, line_min.y + 3.0f * s),
                ImVec2(line_min.x + 128.0f * s + 100.0f * s, line_min.y + 3.0f * s),
                c_color(232, 72, 72, (int)(120 * a)).as_imcolor());

            ImGui::SetCursorPosX(ccx - 128.0f * s);
            if (Button(tr("Выгрузить чит", "Unload cheat")))
            {
                // Defer teardown until the next frame starts, outside the
                // active ImGui button/render call stack.
                Present::RequestUnload();
            }
        }

        ImGui::EndChild(false);
}
