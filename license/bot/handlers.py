"""Логика команд Telegram-бота. Не зависит от Telegram API — тестируется офлайн."""

from __future__ import annotations

from datetime import datetime, timezone

from .api_client import LicenseApiError, LicenseApiClient

HELP = (
    "18:32 cheat — бот выдачи лицензионных ключей.\n"
    "\n"
    "Команды:\n"
    "/gen <дней> [сколько] [заметка] — создать ключи\n"
    "    примеры: /gen 30, /gen 180 5, /gen 365 1 постоянный\n"
    "/keys — последние ключи\n"
    "/info <ключ> — подробности о ключе\n"
    "/revoke <ключ> — отозвать ключ\n"
    "/restore <ключ> — вернуть отозванный ключ\n"
    "/reset <ключ> — отвязать ключ от компьютера\n"
    "\n"
    "Готовые сроки: 30, 180, 365 дней — или любое своё число.\n"
    "Срок ключа начинается с первой активации, до этого ключ «спит»."
)


def _now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _format_date(value: str | None) -> str:
    if not value:
        return "—"
    return f"{value[8:10]}.{value[5:7]}.{value[0:4]}"


def _format_datetime(value: str | None) -> str:
    if not value:
        return "—"
    return f"{value[8:10]}.{value[5:7]}.{value[0:4]} {value[11:16]}"


def _days_word(count: int) -> str:
    if count % 10 == 1 and count % 100 != 11:
        return "день"
    if 2 <= count % 10 <= 4 and (count % 100 < 10 or count % 100 >= 20):
        return "дня"
    return "дней"


def describe_key(record: dict) -> str:
    if record.get("revoked"):
        return "отозван"
    if not record.get("activated_at"):
        return f"не активирован ({record['duration_days']} {_days_word(record['duration_days'])} после активации)"
    if record.get("expires_at", "") <= _now_iso():
        return f"истёк {_format_date(record['expires_at'])}"
    return f"активен до {_format_date(record['expires_at'])}"


class BotHandlers:
    """Принимает (user_id, text), возвращает список ответов."""

    def __init__(self, api: LicenseApiClient, admin_ids: list[int]) -> None:
        self.api = api
        self.admin_ids = {int(user_id) for user_id in admin_ids}

    def handle(self, user_id: int, text: str) -> list[str]:
        text = (text or "").strip()
        if not text:
            return []
        command = text.split()[0].split("@")[0].lower()
        args = text.split()[1:]

        if command in ("/start", "/help"):
            return [HELP]
        if user_id not in self.admin_ids:
            return ["Нет доступа: бот только для администратора."]
        if command == "/gen":
            return self._gen(args)
        if command == "/keys":
            return self._keys()
        if command == "/info":
            return self._info(args)
        if command == "/revoke":
            return self._revoke(args)
        if command == "/restore":
            return self._restore(args)
        if command == "/reset":
            return self._reset(args)
        return ["Неизвестная команда. /help — список команд."]

    # -- команды -------------------------------------------------------

    def _gen(self, args: list[str]) -> list[str]:
        if not args or not args[0].isdigit():
            return [
                "Использование: /gen <дней> [сколько] [заметка]\n"
                "Например: /gen 30 — один ключ на 30 дней, /gen 180 5 — пять ключей на 180."
            ]
        days = int(args[0])
        if not 1 <= days <= 3650:
            return ["Срок — от 1 до 3650 дней."]
        count, rest = 1, args[1:]
        if rest and rest[0].isdigit():
            count, rest = int(rest[0]), rest[1:]
        if not 1 <= count <= 50:
            return ["За один раз — от 1 до 50 ключей."]
        note = " ".join(rest)[:200]

        try:
            data = self.api.create_keys(days, count, note)
        except LicenseApiError as error:
            return [f"Ошибка: {error}"]

        keys = data.get("keys", [])
        lines = [f"Создано {len(keys)} {self._keys_word(len(keys))} на {days} {_days_word(days)}:"]
        for record in keys:
            lines.append(record["key"])
        return ["\n".join(lines)]

    @staticmethod
    def _keys_word(count: int) -> str:
        if count % 10 == 1 and count % 100 != 11:
            return "ключ"
        if 2 <= count % 10 <= 4 and (count % 100 < 10 or count % 100 >= 20):
            return "ключа"
        return "ключей"

    def _keys(self) -> list[str]:
        try:
            records = self.api.list_keys()
        except LicenseApiError as error:
            return [f"Ошибка: {error}"]
        if not records:
            return ["Ключей пока нет. Создайте первый: /gen 30"]
        lines = ["Последние ключи:"]
        for record in records[:20]:
            note = f" · {record['note']}" if record.get("note") else ""
            lines.append(f"{record['key']} · {record['duration_days']} дн. · {describe_key(record)}{note}")
        if len(records) > 20:
            lines.append(f"… и ещё {len(records) - 20}")
        return ["\n".join(lines)]

    def _info(self, args: list[str]) -> list[str]:
        if not args:
            return ["Использование: /info <ключ>"]
        key = args[0].strip().upper()
        try:
            record = self.api.key_info(key)
        except LicenseApiError as error:
            return [f"Ошибка: {error}"]
        if record is None:
            return ["Ключ не найден."]
        hwid = record.get("hwid")
        lines = [
            f"Ключ: {record['key']}",
            f"Срок: {record['duration_days']} {_days_word(record['duration_days'])}",
            f"Статус: {describe_key(record)}",
            f"Активирован: {_format_datetime(record.get('activated_at'))}",
            f"Истекает: {_format_datetime(record.get('expires_at'))}",
            f"HWID: {hwid if hwid else 'не привязан'}",
        ]
        if record.get("note"):
            lines.append(f"Заметка: {record['note']}")
        return ["\n".join(lines)]

    def _key_action(self, args: list[str], action, action_name: str) -> list[str]:
        if not args:
            return [f"Использование: /{action_name} <ключ>"]
        key = args[0].strip().upper()
        try:
            action(key)
            record = self.api.key_info(key)
        except LicenseApiError as error:
            return [f"Ошибка: {error}"]
        if record is None:
            return ["Ключ не найден."]
        return [f"Готово. {record['key']} — {describe_key(record)}"]

    def _revoke(self, args: list[str]) -> list[str]:
        return self._key_action(args, self.api.revoke, "revoke")

    def _restore(self, args: list[str]) -> list[str]:
        return self._key_action(args, self.api.restore, "restore")

    def _reset(self, args: list[str]) -> list[str]:
        return self._key_action(args, self.api.reset_hwid, "reset")
