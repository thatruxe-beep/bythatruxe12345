#pragma once

#include <mutex>
#include <string>
#include <thread>

enum class LicenseState
{
    Idle,          // ключ ещё не проверялся
    Unused,        // ключ существует, срок ещё не запущен
    Active,        // подписка действует
    Expired,       // подписка истекла — доступ заблокирован
    Revoked,       // ключ отозван
    HwidMismatch,  // ключ привязан к другому компьютеру
    Invalid,       // ключ не найден
    ServerError,   // сервер недоступен
};

struct LoaderModel
{
    char key[32] = {};
    std::string server = "http://127.0.0.1:8000";
    std::string gamePath;
    std::string machineId;

    LicenseState state = LicenseState::Idle;
    bool firstActivation = false;
    int daysLeft = 0;
    int durationDays = 0;
    std::string activatedAt;   // 2026-10-02T15:04:05Z
    std::string expiresAt;
    std::string statusText;    // пояснение для интерфейса
    std::string launchError;   // последняя ошибка запуска игры

    bool CanLaunch() const { return state == LicenseState::Active; }
};

// Читает/пишет loader.ini (server, game, key).
void LoadSettings(LoaderModel& model, const std::string& iniPath);
void SaveSettings(const LoaderModel& model, const std::string& iniPath);

// Проверяет ключ на сервере. activate=true — первая активация запускает срок.
void PerformLicenseRequest(LoaderModel& model, bool activate);

// Обычный запуск gta_sa.exe (CreateProcess, без инъекций).
bool LaunchGame(LoaderModel& model, std::string& error);

// Выполняет запросы к серверу в фоне, чтобы интерфейс не замирал.
class LoaderController
{
public:
    ~LoaderController();

    bool Busy() const;
    void Start(LoaderModel model, bool activate);   // модель берётся копией
    bool Pump(LoaderModel& model);                  // true — модель обновлена

private:
    void Run();

    mutable std::mutex mutex_;
    std::thread thread_;
    LoaderModel result_;
    bool busy_ = false;
    bool hasResult_ = false;
    bool activate_ = false;
};
