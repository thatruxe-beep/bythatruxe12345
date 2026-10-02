"""Конфигурация лицензионной системы (сервер + Telegram-бот).

Читает `license/config.json`, каждое значение можно переопределить
переменной окружения. Пример конфигурации — `config.example.json`.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path

LICENSE_ROOT = Path(__file__).resolve().parent.parent

DEFAULTS = {
    "server": {
        "host": "127.0.0.1",
        "port": 8000,
    },
    "database": "licenses.db",
    "admin_token": "change-me-1832",
    "binding": {
        # привязывать ключ к первому компьютеру активации
        "hwid": False,
    },
    "telegram": {
        "bot_token": "",
        "admin_ids": [],
    },
}


def _deep_merge(base: dict, override: dict) -> dict:
    result = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _deep_merge(result[key], value)
        else:
            result[key] = value
    return result


def load_raw_config() -> dict:
    config = dict(DEFAULTS)
    config_path = LICENSE_ROOT / "config.json"
    if config_path.is_file():
        with config_path.open("r", encoding="utf-8") as handle:
            config = _deep_merge(config, json.load(handle))

    env = os.environ
    if env.get("LICENSE_HOST"):
        config["server"]["host"] = env["LICENSE_HOST"]
    if env.get("LICENSE_PORT"):
        config["server"]["port"] = int(env["LICENSE_PORT"])
    if env.get("LICENSE_DB"):
        config["database"] = env["LICENSE_DB"]
    if env.get("LICENSE_ADMIN_TOKEN"):
        config["admin_token"] = env["LICENSE_ADMIN_TOKEN"]
    if env.get("LICENSE_BIND_HWID"):
        config["binding"]["hwid"] = env["LICENSE_BIND_HWID"].strip().lower() in ("1", "true", "yes", "on")
    if env.get("LICENSE_BOT_TOKEN"):
        config["telegram"]["bot_token"] = env["LICENSE_BOT_TOKEN"]
    if env.get("LICENSE_ADMIN_IDS"):
        config["telegram"]["admin_ids"] = [
            int(part.strip())
            for part in env["LICENSE_ADMIN_IDS"].split(",")
            if part.strip().isdigit()
        ]
    return config


@dataclass
class ServerSettings:
    host: str
    port: int
    database_path: Path
    admin_token: str


@dataclass
class TelegramSettings:
    bot_token: str
    admin_ids: list[int] = field(default_factory=list)


def server_settings() -> ServerSettings:
    raw = load_raw_config()
    database = raw["database"]
    database_path = Path(database)
    if not database_path.is_absolute():
        database_path = LICENSE_ROOT / database_path
    return ServerSettings(
        host=raw["server"]["host"],
        port=int(raw["server"]["port"]),
        database_path=database_path,
        admin_token=raw["admin_token"],
    )


def telegram_settings() -> TelegramSettings:
    raw = load_raw_config()
    return TelegramSettings(
        bot_token=raw["telegram"]["bot_token"],
        admin_ids=[int(user_id) for user_id in raw["telegram"]["admin_ids"]],
    )


def server_base_url() -> str:
    """URL, по которому компоненты на этой же машине обращаются к серверу."""
    settings = server_settings()
    host = settings.host
    if host in ("0.0.0.0", "::"):
        host = "127.0.0.1"
    return f"http://{host}:{settings.port}"


def bind_hwid_enabled() -> bool:
    """Привязывать ли ключ к первому ПК, на котором его активировали."""
    return bool(load_raw_config()["binding"]["hwid"])
