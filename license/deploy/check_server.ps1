param(
    [string]$Server = "http://127.0.0.1:8000"
)

# Проверка доступности лицензионного сервера 18:32 cheat.
# Пример: .\check_server.ps1 http://12.34.56.78:8000

$ErrorActionPreference = "Stop"

try {
    $health = Invoke-RestMethod -Uri "$Server/api/health" -TimeoutSec 10
    Write-Host "OK: сервер отвечает, статус = $($health.status), ключей в базе: $($health.keys)" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "FAIL: сервер недоступен ($($_.Exception.Message))" -ForegroundColor Red
    Write-Host "Проверь: VPS включён, сервис запущен (systemctl status 1832-license), порт 8000 открыт." -ForegroundColor Yellow
    exit 1
}
