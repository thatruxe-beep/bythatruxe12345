#include "Menu/Auth.hpp"

#include "imgui.h"

#include <cfloat>
#include <cstdio>

#include "Core/Config.hpp"
#include "Core/License.hpp"
#include "Gfx/Fonts.hpp"

namespace
{
// Палитра Phobia (как в меню и лоадере 18:32).
const ImVec4 kAccent = ImVec4(35.0f / 255.0f, 202.0f / 255.0f, 238.0f / 255.0f, 1.0f);
const ImVec4 kAccentHover = ImVec4(80.0f / 255.0f, 216.0f / 255.0f, 246.0f / 255.0f, 1.0f);
const ImVec4 kAccentDim = ImVec4(24.0f / 255.0f, 138.0f / 255.0f, 163.0f / 255.0f, 1.0f);
const ImVec4 kCard = ImVec4(17.0f / 255.0f, 21.0f / 255.0f, 28.0f / 255.0f, 1.0f);
const ImVec4 kCardBorder = ImVec4(31.0f / 255.0f, 38.0f / 255.0f, 49.0f / 255.0f, 1.0f);
const ImVec4 kFrame = ImVec4(21.0f / 255.0f, 26.0f / 255.0f, 34.0f / 255.0f, 1.0f);
const ImVec4 kText = ImVec4(230.0f / 255.0f, 237.0f / 255.0f, 244.0f / 255.0f, 1.0f);
const ImVec4 kMuted = ImVec4(126.0f / 255.0f, 135.0f / 255.0f, 149.0f / 255.0f, 1.0f);
const ImVec4 kDanger = ImVec4(240.0f / 255.0f, 86.0f / 255.0f, 106.0f / 255.0f, 1.0f);
const ImVec4 kWarning = ImVec4(240.0f / 255.0f, 160.0f / 255.0f, 86.0f / 255.0f, 1.0f);
const ImVec4 kDarkText = ImVec4(8.0f / 255.0f, 13.0f / 255.0f, 18.0f / 255.0f, 1.0f);

ImU32 U32(const ImVec4& color)
{
    return ImGui::GetColorU32(color);
}

ImVec2 Measure(ImFont* font, float size, const char* text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}
}

void Auth::Draw()
{
    ImGuiIO& io = ImGui::GetIO();
    const float s = g_cfg.ui_scale / 100.0f;

    // Затемняем игру позади окна.
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.0f, 0.0f), io.DisplaySize,
        ImGui::GetColorU32(ImVec4(6.0f / 255.0f, 8.0f / 255.0f, 11.0f / 255.0f, 0.93f)));

    const ImVec2 cardSize(430.0f * s, 264.0f * s);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - cardSize.x) * 0.5f,
                                   (io.DisplaySize.y - cardSize.y) * 0.5f));
    ImGui::SetNextWindowSize(cardSize);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, kCard);
    ImGui::PushStyleColor(ImGuiCol_Border, kCardBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kFrame);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, kMuted);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f * s, 20.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * s, 10.0f * s));

    ImGui::Begin("##auth_window", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvailWidth();
    const License::Phase phase = License::GetPhase();
    const bool canType = (phase == License::Phase::NeedKey
                          || phase == License::Phase::Expired
                          || phase == License::Phase::HwidMismatch
                          || phase == License::Phase::ServerError
                          || phase == License::Phase::Revoked);
    // Каждый раз при появлении поля — сразу фокус на него, чтобы можно было
    // печатать ключ без клика мышью.
    static bool sFocusPending = true;

    // -- шапка ----------------------------------------------------------
    const float logo = 44.0f * s;
    draw->AddRectFilled(origin, ImVec2(origin.x + logo, origin.y + logo), U32(kFrame), 9.0f * s);
    const ImVec2 logoSize = Measure(g_fonts.main, 15.0f * s, "18:32");
    draw->AddText(g_fonts.main, 15.0f * s,
                  ImVec2(origin.x + (logo - logoSize.x) * 0.5f, origin.y + (logo - logoSize.y) * 0.5f),
                  U32(kAccent), "18:32");

    const float textX = origin.x + logo + 13.0f * s;
    draw->AddText(g_fonts.main, 17.0f * s, ImVec2(textX, origin.y + 2.0f * s), U32(kText), "18:32 CHEAT");
    draw->AddText(g_fonts.main, 11.0f * s, ImVec2(textX, origin.y + 26.0f * s), U32(kMuted), "АКТИВАЦИЯ ЛИЦЕНЗИИ");
    draw->AddRectFilled(ImVec2(textX, origin.y + 44.0f * s),
                        ImVec2(textX + 34.0f * s, origin.y + 47.0f * s), U32(kAccent));

    // HWID этого ПК — покупатель отправляет его продавцу для получения ключа.
    {
        char hwidText[16] = {};
        License::GetMachineHwidText(hwidText, sizeof(hwidText));
        char hwidLine[48] = {};
        _snprintf_s(hwidLine, _TRUNCATE, "HWID: %s", hwidText);
        const ImVec2 hwidSize = Measure(g_fonts.main, 11.0f * s, hwidLine);
        draw->AddText(g_fonts.main, 11.0f * s,
                      ImVec2(origin.x + width - hwidSize.x, origin.y + 2.0f * s),
                      U32(kMuted), hwidLine);
        draw->AddText(g_fonts.main, 10.0f * s,
                      ImVec2(origin.x + width - hwidSize.x, origin.y + 16.0f * s),
                      U32(kMuted), "отправьте продавцу");
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + logo + 20.0f * s));

    // -- поле ключа -------------------------------------------------------
    if (canType)
    {
        ImGui::SetNextItemWidth(-1.0f);
        if (sFocusPending)
        {
            ImGui::SetKeyboardFocusHere(0);
        }
        const bool enterPressed = ImGui::InputText(
            "##auth_key", License::KeyBuffer(), 64,
            ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank |
                ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        sFocusPending = false;

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentDim);
        ImGui::PushStyleColor(ImGuiCol_Text, kDarkText);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f * s));
        const bool clicked = ImGui::Button("АКТИВИРОВАТЬ", ImVec2(width, 38.0f * s));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        if (enterPressed || clicked)
        {
            License::SubmitKey();
        }
    }
    else
    {
        sFocusPending = true;
        ImGui::Dummy(ImVec2(width, 38.0f * s));
    }

    // -- статус -----------------------------------------------------------
    ImGui::Spacing();
    ImGui::Spacing();

    char status[160] = {};
    ImVec4 statusColor = kMuted;

    if (phase == License::Phase::NeedKey)
    {
        _snprintf_s(status, _TRUNCATE, "%s", "Введите лицензионный ключ");
    }
    else if (phase == License::Phase::Expired)
    {
        _snprintf_s(status, _TRUNCATE, "%s", "Срок ключа истёк — введите новый ключ");
        statusColor = kWarning;
    }
    else if (phase == License::Phase::HwidMismatch)
    {
        _snprintf_s(status, _TRUNCATE, "%s", "Ключ привязан к другому компьютеру");
        statusColor = kWarning;
    }
    else if (phase == License::Phase::ServerError)
    {
        _snprintf_s(status, _TRUNCATE, "%s",
            "Сервер лицензий недоступен. Попробуйте ещё раз позже");
        statusColor = kWarning;
    }
    else if (phase == License::Phase::Revoked)
    {
        _snprintf_s(status, _TRUNCATE, "%s", "Ключ отозван. Вопросы: t.me/thatruxe");
        statusColor = kWarning;
    }
    else if (phase == License::Phase::WrongKey)
    {
        const int seconds = License::SecondsToExit();
        _snprintf_s(status, _TRUNCATE, "Неверный ключ. Игра закроется через %d...",
                    seconds > 0 ? seconds : 0);
        statusColor = kDanger;
    }
    else if (phase == License::Phase::Blocked)
    {
        const int seconds = License::SecondsToExit();
        _snprintf_s(status, _TRUNCATE, "Системное время переведено назад. Игра закроется через %d...",
                    seconds > 0 ? seconds : 0);
        statusColor = kDanger;
    }

    if (status[0] != '\0')
    {
        draw->AddText(g_fonts.main, 13.0f * s, ImGui::GetCursorScreenPos(), U32(statusColor), status);
    }

    // -- подсказка формата --------------------------------------------------
    const char* hint = "формат: XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX";
    const char* contact = "ключи: t.me/thatruxe";
    const ImVec2 contactSize = Measure(g_fonts.main, 11.0f * s, contact);
    const float bottom = origin.y + cardSize.y - 20.0f * s;
    draw->AddText(g_fonts.main, 11.0f * s, ImVec2(origin.x, bottom), U32(kMuted), hint);
    draw->AddText(g_fonts.main, 11.0f * s,
                  ImVec2(origin.x + width - contactSize.x, bottom),
                  U32(kMuted), contact);

    ImGui::End(false);

    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(5);
}
