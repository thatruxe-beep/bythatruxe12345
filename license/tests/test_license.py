"""Офлайн-тесты лицензионной системы: сервер + логика бота."""

import os
import re
import tempfile
from pathlib import Path

# Окружение настраиваем ДО импорта приложения.
_TMP = Path(tempfile.mkdtemp(prefix="license-tests-"))
os.environ["LICENSE_DB"] = str(_TMP / "licenses.db")
os.environ["LICENSE_ADMIN_TOKEN"] = "test-admin-token"
os.environ["LICENSE_BIND_HWID"] = "1"

from fastapi.testclient import TestClient  # noqa: E402

from license.bot.api_client import LicenseApiError  # noqa: E402
from license.bot.handlers import BotHandlers  # noqa: E402
from license.server.db import to_iso, utcnow  # noqa: E402
from license.server.main import create_app  # noqa: E402

ADMIN = {"Authorization": "Bearer test-admin-token"}
KEY_FORMAT = re.compile(r"^[A-Z2-9]{4}(-[A-Z2-9]{4}){3}$")
HWID_A = "AAAA1111BBBB2222"
HWID_B = "CCCC3333DDDD4444"


def make_client() -> TestClient:
    return TestClient(create_app())


def create_keys(client: TestClient, days: int, count: int = 1, note: str = "") -> list[dict]:
    response = client.post(
        "/api/admin/keys",
        json={"days": days, "count": count, "note": note},
        headers=ADMIN,
    )
    assert response.status_code == 200, response.text
    return response.json()["keys"]


# -- API ----------------------------------------------------------------


def test_health_and_admin_auth() -> None:
    client = make_client()
    assert client.get("/api/health").json()["status"] == "ok"
    assert client.get("/api/admin/keys").status_code == 401
    assert client.get("/api/admin/keys", headers=ADMIN).status_code == 200


def test_key_format_and_custom_days() -> None:
    client = make_client()
    keys = create_keys(client, days=45)
    assert KEY_FORMAT.fullmatch(keys[0]["key"])
    assert keys[0]["duration_days"] == 45


def test_subscription_starts_on_first_activation() -> None:
    client = make_client()
    key = create_keys(client, days=30)[0]["key"]

    # до активации ключ «спит»
    status = client.post("/api/status", json={"key": key, "hwid": HWID_A})
    assert status.status_code == 200
    assert status.json()["status"] == "unused"

    # первая активация запускает срок
    first = client.post("/api/activate", json={"key": key, "hwid": HWID_A})
    assert first.status_code == 200
    body = first.json()
    assert body["status"] == "ok"
    assert body["first"] is True
    assert body["days_left"] == 30
    assert body["activated_at"] is not None
    assert body["expires_at"] is not None

    # повторный запуск не продлевает срок
    second = client.post("/api/activate", json={"key": key, "hwid": HWID_A})
    assert second.status_code == 200
    assert second.json()["first"] is False
    assert second.json()["expires_at"] == body["expires_at"]


def test_invalid_key_rejected() -> None:
    client = make_client()
    response = client.post("/api/activate", json={"key": "XXXX-XXXX-XXXX-XXXX", "hwid": HWID_A})
    assert response.status_code == 404
    assert response.json()["status"] == "invalid"


def test_expired_key_is_blocked() -> None:
    client = make_client()
    key = create_keys(client, days=30)[0]["key"]
    assert client.post("/api/activate", json={"key": key, "hwid": HWID_A}).status_code == 200

    # имитируем истечение срока
    app = client.app
    store = app.state.store
    past = to_iso(utcnow().replace(microsecond=0))
    store._execute(
        "UPDATE license_keys SET expires_at = ? WHERE key = ?", (past, key)
    )

    status = client.post("/api/status", json={"key": key, "hwid": HWID_A})
    assert status.status_code == 403
    assert status.json()["status"] == "expired"

    activate = client.post("/api/activate", json={"key": key, "hwid": HWID_A})
    assert activate.status_code == 403
    assert activate.json()["status"] == "expired"


def test_revoked_key_is_blocked() -> None:
    client = make_client()
    key = create_keys(client, days=30)[0]["key"]
    assert client.post("/api/activate", json={"key": key, "hwid": HWID_A}).status_code == 200

    assert client.post(f"/api/admin/keys/{key}/revoke", headers=ADMIN).status_code == 200
    response = client.post("/api/activate", json={"key": key, "hwid": HWID_A})
    assert response.status_code == 403
    assert response.json()["status"] == "revoked"

    # восстановление возвращает доступ
    assert client.post(f"/api/admin/keys/{key}/restore", headers=ADMIN).status_code == 200
    assert client.post("/api/activate", json={"key": key, "hwid": HWID_A}).status_code == 200


def test_hwid_binding_and_reset() -> None:
    client = make_client()
    key = create_keys(client, days=180)[0]["key"]
    assert client.post("/api/activate", json={"key": key, "hwid": HWID_A}).status_code == 200

    # другой ПК не может пользоваться ключом
    other = client.post("/api/activate", json={"key": key, "hwid": HWID_B})
    assert other.status_code == 403
    assert other.json()["status"] == "hwid_mismatch"

    # администратор отвязывает ключ — новый ПК теперь работает
    assert client.post(f"/api/admin/keys/{key}/reset-hwid", headers=ADMIN).status_code == 200
    rebound = client.post("/api/activate", json={"key": key, "hwid": HWID_B})
    assert rebound.status_code == 200


def test_presets_30_180_365() -> None:
    client = make_client()
    for days in (30, 180, 365):
        key = create_keys(client, days=days)[0]["key"]
        response = client.post("/api/activate", json={"key": key, "hwid": HWID_A})
        assert response.status_code == 200
        assert response.json()["days_left"] == days


# -- бот ----------------------------------------------------------------


class FakeApi:
    """Заглушка клиента сервера для проверки команд бота."""

    def __init__(self) -> None:
        self.records: list[dict] = []
        self.counter = 0

    def create_keys(self, days: int, count: int = 1, note: str = "") -> dict:
        keys = []
        for _ in range(count):
            self.counter += 1
            keys.append(
                {
                    "key": f"TEST-{self.counter:04d}-KEY{days}",
                    "duration_days": days,
                    "note": note,
                    "created_at": "2026-10-02T10:00:00Z",
                    "activated_at": None,
                    "expires_at": None,
                    "hwid": None,
                    "revoked": 0,
                }
            )
        self.records = keys + self.records
        return {"status": "ok", "keys": keys}

    def list_keys(self) -> list[dict]:
        return self.records

    def key_info(self, key: str) -> dict | None:
        for record in self.records:
            if record["key"] == key:
                return record
        return None

    def revoke(self, key: str) -> dict:
        for record in self.records:
            if record["key"] == key:
                record["revoked"] = 1
        return {"status": "ok"}

    def restore(self, key: str) -> dict:
        for record in self.records:
            if record["key"] == key:
                record["revoked"] = 0
        return {"status": "ok"}

    def reset_hwid(self, key: str) -> dict:
        return {"status": "ok"}


def make_bot() -> tuple[BotHandlers, FakeApi]:
    api = FakeApi()
    return BotHandlers(api, admin_ids=[100]), api


def test_bot_help() -> None:
    bot, _ = make_bot()
    replies = bot.handle(100, "/start")
    assert len(replies) == 1
    assert "/gen" in replies[0]


def test_bot_denies_strangers() -> None:
    bot, _ = make_bot()
    assert "Нет доступа" in bot.handle(999, "/gen 30")[0]


def test_bot_generates_keys() -> None:
    bot, _ = make_bot()
    reply = bot.handle(100, "/gen 30")[0]
    assert "TEST-0001-KEY30" in reply
    assert "30" in reply

    reply = bot.handle(100, "/gen 180 3 покупатель")[0]
    for index in (2, 3, 4):
        assert f"TEST-000{index}-KEY180" in reply

    assert "от 1 до 3650" in bot.handle(100, "/gen 0")[0]
    assert "Использование" in bot.handle(100, "/gen")[0]


def test_bot_lists_and_info() -> None:
    bot, api = make_bot()
    bot.handle(100, "/gen 365 1 постоянный")
    listing = bot.handle(100, "/keys")[0]
    assert "TEST-0001-KEY365" in listing
    assert "не активирован" in listing

    info = bot.handle(100, "/info TEST-0001-KEY365")[0]
    assert "365" in info
    assert "постоянный" in info
    assert "Ключ не найден" in bot.handle(100, "/info XXXX")[0]


def test_bot_revokes() -> None:
    bot, _ = make_bot()
    bot.handle(100, "/gen 30")
    reply = bot.handle(100, "/revoke TEST-0001-KEY30")[0]
    assert "Готово" in reply
    assert "отозван" in reply


def test_bot_reports_server_error() -> None:
    class BrokenApi(FakeApi):
        def create_keys(self, days, count=1, note=""):
            raise LicenseApiError("Сервер лицензий недоступен (ConnectError).")

    bot = BotHandlers(BrokenApi(), admin_ids=[100])
    assert "Ошибка" in bot.handle(100, "/gen 30")[0]


# -- авто-регистрация подписанных ключей + привязка к ПК -----------------


def test_pre_generated_key_auto_registers_and_binds() -> None:
    """Ключ из genkeys.py: первая активация привязывает его к ПК,
    на чужом ПК тот же ключ не работает."""
    import sys
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from keylib import make_key

    client = make_client()
    key = make_key(30 * 1440)  # универсальный ключ на 30 дней, как из стока

    # первой активации нет в базе — сервер регистрирует и активирует
    first = client.post("/api/activate", json={"key": key, "hwid": "AAAA1111BBBB2222"})
    assert first.status_code == 200, first.text
    body = first.json()
    assert body["status"] == "ok"
    assert body["first"] is True
    assert body["days_left"] == 30

    # тот же ПК — работает
    again = client.post("/api/activate", json={"key": key, "hwid": "AAAA1111BBBB2222"})
    assert again.status_code == 200
    assert again.json()["first"] is False

    # чужой ПК — отказ
    other = client.post("/api/activate", json={"key": key, "hwid": "CCCC3333DDDD4444"})
    assert other.status_code == 403
    assert other.json()["status"] == "hwid_mismatch"

    # статус с чужого ПК тоже отказ
    other_status = client.post("/api/status", json={"key": key, "hwid": "CCCC3333DDDD4444"})
    assert other_status.status_code == 403


def test_five_minute_key_expires_by_minutes() -> None:
    import sys
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from keylib import make_key

    client = make_client()
    key = make_key(5)  # 5 минут

    first = client.post("/api/activate", json={"key": key, "hwid": "AAAA1111BBBB2222"})
    assert first.status_code == 200
    assert first.json()["status"] == "ok"

    # имитируем истечение 5-минутного срока
    store = client.app.state.store
    past = "2020-01-01T00:00:00Z"
    store._execute("UPDATE license_keys SET expires_at = ? WHERE key = ?", (past, key))

    expired = client.post("/api/activate", json={"key": key, "hwid": "AAAA1111BBBB2222"})
    assert expired.status_code == 403
    assert expired.json()["status"] == "expired"


def test_unsigned_garbage_not_registered() -> None:
    client = make_client()
    response = client.post("/api/activate", json={"key": "XXXX-XXXX-XXXX-XXXX", "hwid": "x"})
    assert response.status_code == 404
    assert response.json()["status"] == "invalid"
