#pragma once

#include <string>

namespace LicenseNet
{
    struct ActivateResult
    {
        bool networkOk = false; // false — сервер недоступен
        unsigned httpCode = 0;
        std::string status;     // ok / expired / revoked / hwid_mismatch / invalid
        bool first = false;
        int daysLeft = 0;
    };

    // Адрес лицензионного сервера: файл %APPDATA%\18_32_cheat\server.txt,
    // если существует, иначе константа из LicenseNet.cpp.
    std::string ServerUrl();

    // POST /api/activate {key, hwid}.
    bool Activate(const std::string& key, const std::string& hwid, ActivateResult& out);
}
