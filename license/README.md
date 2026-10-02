# Лицензионная система «18:32 cheat»

Локальная система ключей: FastAPI-сервер + SQLite + Telegram-бот для выдачи ключей
и Windows-загрузчик (`18_32_loader/`) с проверкой подписки.

Всё работает на одной машине: сервер и бот запускаются локально, loader обращается
к `http://127.0.0.1:8000`.

## Возможности

- Ключи на **30 / 180 / 365** дней и на **любое своё число дней**.
- Срок начинается **с первой активации** — до этого ключ «спит».
- Loader показывает **остаток дней**, дату окончания и полосу срока.
- После окончания подписки доступ **блокируется** (статус `expired`).
- Отзыв ключа (`revoked`) и привязка к ПК (`hwid`, опционально).
- Telegram-бот: создание, просмотр, отзыв и отвязка ключей.

## Состав

```
license/
  server/          FastAPI + SQLite (API ключей)
  bot/             Telegram-бот (long polling)
  tests/           офлайн-тесты (pytest)
  config.example.json
18_32_loader/      Windows-загрузчик (ImGui, Phobia-стиль)
```

## Установка

1. Python 3.10+.
2. `pip install -r license/requirements.txt`
3. Скопируйте `license/config.example.json` в `license/config.json` и заполните:
   - `telegram.bot_token` — токен от [@BotFather](https://t.me/BotFather);
   - `telegram.admin_ids` — ваш числовой id (узнать у [@userinfobot](https://t.me/userinfobot));
   - `admin_token` — секрет для админ-запросов к API (поменяйте);
   - `binding.hwid` — `true`, если ключ должен привязываться к первому ПК.

## Запуск

```powershell
python -m license.server.run   # лицензионный сервер: http://127.0.0.1:8000
python -m license.bot.run      # Telegram-бот (в отдельном окне)
```

Либо двойным кликом: `license\start-server.bat` и `license\start-bot.bat`
(запускать из двух разных окон — сервер и бот работают одновременно).

## Команды бота

| Команда | Что делает |
| --- | --- |
| `/gen <дней> [сколько] [заметка]` | создать ключи (`/gen 30`, `/gen 180 5`, `/gen 365 1 постоянный`) |
| `/keys` | последние 20 ключей |
| `/info <ключ>` | подробности: срок, статус, активация, HWID |
| `/revoke <ключ>` | отозвать ключ |
| `/restore <ключ>` | вернуть отозванный ключ |
| `/reset <ключ>` | отвязать ключ от компьютера |

## API

| Метод | Описание |
| --- | --- |
| `POST /api/activate` `{key, hwid}` | активация; **первая активация запускает срок** |
| `POST /api/status` `{key, hwid}` | проверка без активации |
| `POST /api/admin/keys` `{days, count, note}` | создать ключи (Bearer `admin_token`) |
| `GET /api/admin/keys` | список ключей |
| `POST /api/admin/keys/<key>/revoke` | отозвать |
| `POST /api/admin/keys/<key>/restore` | восстановить |
| `POST /api/admin/keys/<key>/reset-hwid` | отвязать от ПК |

Ответы: `ok` (с `days_left`, `expires_at`), `unused`, `expired`, `revoked`,
`hwid_mismatch`, `invalid`.

## Загрузчик

Сборка (Windows, Developer PowerShell for VS):

```powershell
.\tools\build-loader.ps1        # build\18_32_loader.exe
```

Рядом с `18_32_loader.exe` положите `loader.ini` (пример — `18_32_loader/loader.ini.example`):

```ini
[loader]
server=http://127.0.0.1:8000
game=C:\Games\GTA San Andreas\gta_sa.exe
```

Загрузчик проверяет ключ, показывает остаток срока и запускает игру обычным
образом. Истёкшая подписка блокирует запуск.

## Тесты

```powershell
python -m pytest license/tests -q
```
