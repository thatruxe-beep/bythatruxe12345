#include "Ui.hpp"

#include "imgui.h"

#include "Loader.hpp"

#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
ImFont* s_fontRegular = nullptr;
ImFont* s_fontBold = nullptr;
ImFont* s_fontBig = nullptr;

// Палитра Phobia, как в меню 18:32 cheat.
const ImVec4 kAccent = ImVec4(35.0f / 255.0f, 202.0f / 255.0f, 238.0f / 255.0f, 1.0f);
const ImVec4 kAccentDim = ImVec4(24.0f / 255.0f, 138.0f / 255.0f, 163.0f / 255.0f, 1.0f);
const ImVec4 kBackground = ImVec4(11.0f / 255.0f, 14.0f / 255.0f, 19.0f / 255.0f, 1.0f);
const ImVec4 kCard = ImVec4(17.0f / 255.0f, 21.0f / 255.0f, 28.0f / 255.0f, 1.0f);
const ImVec4 kCardBorder = ImVec4(31.0f / 255.0f, 38.0f / 255.0f, 49.0f / 255.0f, 1.0f);
const ImVec4 kFrame = ImVec4(21.0f / 255.0f, 26.0f / 255.0f, 34.0f / 255.0f, 1.0f);
const ImVec4 kText = ImVec4(230.0f / 255.0f, 237.0f / 255.0f, 244.0f / 255.0f, 1.0f);
const ImVec4 kMuted = ImVec4(126.0f / 255.0f, 135.0f / 255.0f, 149.0f / 255.0f, 1.0f);
const ImVec4 kDanger = ImVec4(240.0f / 255.0f, 86.0f / 255.0f, 106.0f / 255.0f, 1.0f);
const ImVec4 kWarning = ImVec4(240.0f / 255.0f, 160.0f / 255.0f, 86.0f / 255.0f, 1.0f);
const ImVec4 kOk = ImVec4(110.0f / 255.0f, 231.0f / 255.0f, 160.0f / 255.0f, 1.0f);
const ImVec4 kDarkText = ImVec4(8.0f / 255.0f, 13.0f / 255.0f, 18.0f / 255.0f, 1.0f);

const char* DaysWord(int count)
{
    if (count % 10 == 1 && count % 100 != 11)
        return "день";
    if (count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20))
        return "дня";
    return "дней";
}

// "2026-11-01T15:04:05Z" -> "01.11.2026"
void FormatDate(const std::string& iso, char* out, size_t size, bool withTime)
{
    out[0] = '\0';
    if (iso.size() < 10)
        return;
    if (withTime && iso.size() >= 16)
        _snprintf_s(out, size, _TRUNCATE, "%.*s.%.*s.%.*s %.*s:%.*s",
                    2, iso.c_str() + 8, 2, iso.c_str() + 5, 4, iso.c_str() + 0,
                    2, iso.c_str() + 11, 2, iso.c_str() + 14);
    else
        _snprintf_s(out, size, _TRUNCATE, "%.*s.%.*s.%.*s",
                    2, iso.c_str() + 8, 2, iso.c_str() + 5, 4, iso.c_str() + 0);
}

ImU32 U32(const ImVec4& color)
{
    return ImGui::GetColorU32(color);
}

ImVec2 TextSize(ImFont* font, float size, const char* text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}
}

bool SetupLoaderFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();

    const char* candidatesRegular[] = {"C:\\Windows\\Fonts\\segoeui.ttf",
                                       "C:\\Windows\\Fonts\\tahoma.ttf"};
    const char* candidatesBold[] = {"C:\\Windows\\Fonts\\segoeuib.ttf",
                                    "C:\\Windows\\Fonts\\tahomabd.ttf"};

    for (const char* path : candidatesRegular)
    {
        s_fontRegular = io.Fonts->AddFontFromFileTTF(path, 15.0f * scale, nullptr, ranges);
        if (s_fontRegular)
            break;
    }
    for (const char* path : candidatesBold)
    {
        s_fontBold = io.Fonts->AddFontFromFileTTF(path, 17.0f * scale, nullptr, ranges);
        if (s_fontBold)
            break;
    }
    if (!s_fontRegular)
        s_fontRegular = io.Fonts->AddFontDefault();
    if (!s_fontBold)
    {
        s_fontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 17.0f * scale);
        if (!s_fontBold)
            s_fontBold = s_fontRegular;
    }
    s_fontBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 36.0f * scale, nullptr, ranges);
    if (!s_fontBig)
        s_fontBig = s_fontBold;
    return s_fontRegular != nullptr;
}

void SetupLoaderStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 7.0f;
    style.ButtonRounding = 7.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = kBackground;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kMuted;
    colors[ImGuiCol_FrameBg] = kFrame;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(26.0f / 255.0f, 33.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(32.0f / 255.0f, 41.0f / 255.0f, 54.0f / 255.0f, 1.0f);
    colors[ImGuiCol_Button] = kAccent;
    colors[ImGuiCol_ButtonHovered] = ImVec4(80.0f / 255.0f, 216.0f / 255.0f, 246.0f, 1.0f);
    colors[ImGuiCol_ButtonActive] = kAccentDim;
    colors[ImGuiCol_TextSelectedBg] = kAccentDim;
}

namespace
{
void DrawAccentButton(const char* label, const ImVec2& size, bool enabled, bool& clicked)
{
    if (enabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(80.0f / 255.0f, 216.0f / 255.0f, 246.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentDim);
        ImGui::PushStyleColor(ImGuiCol_Text, kDarkText);
        clicked = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(32.0f / 255.0f, 38.0f / 255.0f, 47.0f / 255.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(32.0f / 255.0f, 38.0f / 255.0f, 47.0f / 255.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(32.0f / 255.0f, 38.0f / 255.0f, 47.0f / 255.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(91.0f / 255.0f, 100.0f / 255.0f, 114.0f / 255.0f, 1.0f));
    ImGui::Button(label, size);   // нажатие не срабатывает
    clicked = false;
    ImGui::PopStyleColor(4);
}

struct CardVisual
{
    const char* title;
    ImVec4 color;
};

CardVisual CardFor(const LoaderModel& model)
{
    switch (model.state)
    {
    case LicenseState::Active:
        return {"ПОДПИСКА АКТИВНА", kOk};
    case LicenseState::Unused:
        return {"КЛЮЧ ГОТОВ К АКТИВАЦИИ", kText};
    case LicenseState::Expired:
        return {"ПОДПИСКА ИСТЕКЛА", kDanger};
    case LicenseState::Revoked:
        return {"КЛЮЧ ОТЗВАН", kDanger};
    case LicenseState::HwidMismatch:
        return {"КЛЮЧ ПРИВЯЗАН К ДРУГОМУ ПК", kWarning};
    case LicenseState::Invalid:
        return {"КЛЮЧ НЕ НАЙДЕН", kWarning};
    case LicenseState::ServerError:
        return {"СЕРВЕР НЕДОСТУПЕН", kWarning};
    default:
        return {"ЛИЦЕНЗИЯ", kMuted};
    }
}
}

void RenderLoader(LoaderModel& model, LoaderController& controller, float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool busy = controller.Busy();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f * scale, 20.0f * scale));
    ImGui::Begin("##loader", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvailWidth();
    const ImVec2 windowBottom = ImVec2(origin.x + width, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);

    // -- шапка ---------------------------------------------------------
    const float logo = 46.0f * scale;
    draw->AddRectFilled(origin, ImVec2(origin.x + logo, origin.y + logo), U32(kCard), 10.0f * scale);
    const ImVec2 logoTextSize = TextSize(s_fontBold, 15.0f * scale, "18:32");
    draw->AddText(s_fontBold, 15.0f * scale,
                  ImVec2(origin.x + (logo - logoTextSize.x) * 0.5f, origin.y + (logo - logoTextSize.y) * 0.5f),
                  U32(kAccent), "18:32");

    const float textX = origin.x + logo + 13.0f * scale;
    draw->AddText(s_fontBold, 20.0f * scale, ImVec2(textX, origin.y + 1.0f * scale), U32(kText), "18:32 CHEAT");
    draw->AddText(s_fontRegular, 12.0f * scale, ImVec2(textX, origin.y + 27.0f * scale), U32(kMuted), "LICENSE LOADER");
    draw->AddRectFilled(ImVec2(textX, origin.y + 46.0f * scale),
                        ImVec2(textX + 36.0f * scale, origin.y + 49.0f * scale), U32(kAccent));

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + logo + 20.0f * scale));

    // -- ввод ключа ------------------------------------------------------
    ImGui::PushFont(s_fontRegular);
    ImGui::TextDisabled("ЛИЦЕНЗИОННЫЙ КЛЮЧ");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * scale, 10.0f * scale));
    ImGui::SetNextItemWidth(-1.0f);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsUppercase |
                                ImGuiInputTextFlags_CharsNoBlank |
                                ImGuiInputTextFlags_AutoSelectAll;
    if (busy)
        flags |= ImGuiInputTextFlags_ReadOnly;
    ImGui::InputText("##key", model.key, sizeof(model.key), flags);
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::TextDisabled("ФОРМАТ: XXXX-XXXX-XXXX-XXXX");
    ImGui::PopFont();

    // -- кнопки -----------------------------------------------------------
    const float buttonHeight = 38.0f * scale;
    const float refreshWidth = 116.0f * scale;
    ImGui::Spacing();

    bool activateClicked = false;
    ImGui::PushFont(s_fontBold);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 9.0f * scale));
    DrawAccentButton(busy ? "ПРОВЕРКА..." : "АКТИВИРОВАТЬ",
                     ImVec2(width - refreshWidth - 10.0f * scale, buttonHeight),
                     !busy && strlen(model.key) >= 4, activateClicked);
    if (activateClicked)
    {
        model.launchError.clear();
        controller.Start(model, true);
    }

    ImGui::SameLine(0.0f, 10.0f * scale);
    bool refreshClicked = false;
    {
        ImGui::PushStyleColor(ImGuiCol_Button, kFrame);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(26.0f / 255.0f, 33.0f / 255.0f, 44.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kCardBorder);
        ImGui::PushStyleColor(ImGuiCol_Text, kText);
        refreshClicked = ImGui::Button("ПРОВЕРИТЬ", ImVec2(refreshWidth, buttonHeight));
        ImGui::PopStyleColor(4);
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
    if (refreshClicked && !busy && strlen(model.key) >= 4)
    {
        model.launchError.clear();
        controller.Start(model, false);
    }

    // -- карточка статуса ---------------------------------------------------
    ImGui::Spacing();
    ImGui::Spacing();

    const float cardHeight = 128.0f * scale;
    const ImVec2 cardMin = ImGui::GetCursorScreenPos();
    const ImVec2 cardMax(cardMin.x + width, cardMin.y + cardHeight);
    draw->AddRectFilled(cardMin, cardMax, U32(kCard), 10.0f * scale);
    draw->AddRect(cardMin, cardMax, U32(kCardBorder), 10.0f * scale);

    const CardVisual visual = CardFor(model);
    const float pad = 18.0f * scale;
    ImVec2 cursor(cardMin.x + pad, cardMin.y + 15.0f * scale);

    if (busy)
    {
        const int dots = static_cast<int>(ImGui::GetTime() * 3.0f) % 4;
        char label[32];
        _snprintf_s(label, _TRUNCATE, "Проверяем ключ%.*s", dots, "...");
        draw->AddText(s_fontBold, 15.0f * scale, cursor, U32(kText), label);
    }
    else if (model.state == LicenseState::Active)
    {
        draw->AddText(s_fontRegular, 11.0f * scale, cursor, U32(visual.color), visual.title);

        char daysText[16];
        _snprintf_s(daysText, _TRUNCATE, "%d", model.daysLeft);
        const ImVec2 daysSize = TextSize(s_fontBig, 36.0f * scale, daysText);
        draw->AddText(s_fontBig, 36.0f * scale, ImVec2(cursor.x, cursor.y + 14.0f * scale), U32(kAccent), daysText);
        draw->AddText(s_fontRegular, 13.0f * scale,
                      ImVec2(cursor.x + daysSize.x + 9.0f * scale, cursor.y + 34.0f * scale),
                      U32(kMuted), DaysWord(model.daysLeft));

        char until[40] = {};
        FormatDate(model.expiresAt, until, sizeof(until), true);
        char activatedLine[64] = {};
        FormatDate(model.activatedAt, activatedLine, sizeof(activatedLine), true);
        char right[96] = {};
        _snprintf_s(right, _TRUNCATE, "до %s", until);
        const ImVec2 rightSize = TextSize(s_fontRegular, 12.0f * scale, right);
        draw->AddText(s_fontRegular, 12.0f * scale,
                      ImVec2(cardMax.x - pad - rightSize.x, cursor.y + 2.0f * scale), U32(kText), right);
        _snprintf_s(right, _TRUNCATE, "активирован %s", activatedLine);
        const ImVec2 rightSize2 = TextSize(s_fontRegular, 12.0f * scale, right);
        draw->AddText(s_fontRegular, 12.0f * scale,
                      ImVec2(cardMax.x - pad - rightSize2.x, cursor.y + 20.0f * scale), U32(kMuted), right);

        // полоса остатка срока
        const float barWidth = width - pad * 2.0f;
        const float barY = cardMax.y - 20.0f * scale;
        float fraction = 1.0f;
        if (model.durationDays > 0)
        {
            fraction = static_cast<float>(model.daysLeft) / static_cast<float>(model.durationDays);
            if (fraction < 0.0f)
                fraction = 0.0f;
            if (fraction > 1.0f)
                fraction = 1.0f;
        }
        draw->AddRectFilled(ImVec2(cardMin.x + pad, barY), ImVec2(cardMin.x + pad + barWidth, barY + 5.0f * scale),
                            U32(kCardBorder), 2.5f * scale);
        if (fraction > 0.0f)
            draw->AddRectFilled(ImVec2(cardMin.x + pad, barY),
                                ImVec2(cardMin.x + pad + barWidth * fraction, barY + 5.0f * scale),
                                U32(kAccent), 2.5f * scale);

        if (model.firstActivation)
            draw->AddText(s_fontRegular, 12.0f * scale,
                          ImVec2(cardMin.x + pad, cardMax.y - 40.0f * scale), U32(kOk),
                          "Ключ активирован — срок пошёл с сегодня.");
    }
    else
    {
        draw->AddText(s_fontBold, 15.0f * scale, cursor, U32(visual.color), visual.title);
        const char* body = model.statusText.empty()
                               ? "Введите ключ и нажмите «Активировать»."
                               : model.statusText.c_str();
        draw->AddText(s_fontRegular, 13.0f * scale, ImVec2(cursor.x, cursor.y + 26.0f * scale), U32(kMuted), body,
                      body + strlen(body), width - pad * 2.0f);
        if (model.state == LicenseState::Expired || model.state == LicenseState::Revoked)
            draw->AddText(s_fontRegular, 13.0f * scale, ImVec2(cursor.x, cursor.y + 48.0f * scale), U32(kAccent),
                          "Продление: t.me/thatruxe");
    }

    // -- запуск игры ---------------------------------------------------------
    ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardMax.y + 16.0f * scale));
    const bool canLaunch = model.CanLaunch() && !busy && !model.gamePath.empty();
    bool launchClicked = false;
    ImGui::PushFont(s_fontBold);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 12.0f * scale));
    DrawAccentButton("ЗАПУСТИТЬ GTA: SA", ImVec2(width, 44.0f * scale), canLaunch, launchClicked);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    if (launchClicked)
    {
        if (!LaunchGame(model, model.launchError))
        {
            // ошибка останется видимой под кнопкой
        }
        else
        {
            model.launchError.clear();
        }
    }

    if (!model.launchError.empty())
    {
        ImGui::Spacing();
        ImGui::PushFont(s_fontRegular);
        ImGui::TextColored(kDanger, "%s", model.launchError.c_str());
        ImGui::PopFont();
    }

    // -- подвал ---------------------------------------------------------------
    char footer[192];
    _snprintf_s(footer, _TRUNCATE, "сервер: %s   ·   t.me/thatruxe", model.server.c_str());
    draw->AddText(s_fontRegular, 11.0f * scale,
                  ImVec2(origin.x, windowBottom.y - 20.0f * scale), U32(kMuted), footer);

    ImGui::End();
    ImGui::PopStyleVar();
}
