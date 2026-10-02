"""SQLite-хранилище лицензионных ключей.

Ключ живёт в трёх состояниях:
- «не активирован» — создан, но срок ещё не идёт;
- «активен» — активирован, срок тикает от `activated_at` до `expires_at`;
- «истёк»/«отозван» — доступ блокируется.
"""

from __future__ import annotations

import math
import secrets
import sqlite3
import threading
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Optional

# Алфавит без похожих символов (0/O, 1/I исключены).
KEY_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
KEY_GROUP_LENGTH = 4
KEY_GROUPS = 4
MAX_GENERATION_ATTEMPTS = 64

SCHEMA = """
CREATE TABLE IF NOT EXISTS license_keys (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    key           TEXT    NOT NULL UNIQUE,
    duration_days INTEGER NOT NULL,
    note          TEXT    NOT NULL DEFAULT '',
    created_at    TEXT    NOT NULL,
    activated_at  TEXT,
    expires_at    TEXT,
    hwid          TEXT,
    activations   INTEGER NOT NULL DEFAULT 0,
    revoked       INTEGER NOT NULL DEFAULT 0
);
"""


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def to_iso(moment: datetime) -> str:
    return moment.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_iso(value: str) -> datetime:
    return datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)


def normalize_key(key: str) -> str:
    return "".join(part for part in key.strip().upper() if not part.isspace())


def days_left(expires_at: str) -> int:
    """Сколько полных дней осталось (последний день = 1)."""
    delta = (parse_iso(expires_at) - utcnow()).total_seconds()
    return max(0, math.ceil(delta / 86400.0))


class LicenseStore:
    """Потокобезопасное хранилище ключей на SQLite."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self._lock = threading.RLock()
        self._connection: sqlite3.Connection | None = None

    # -- внутреннее ---------------------------------------------------

    def _connect(self) -> sqlite3.Connection:
        if self._connection is None:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self._connection = sqlite3.connect(str(self.path), check_same_thread=False)
            self._connection.row_factory = sqlite3.Row
            self._connection.executescript(SCHEMA)
            self._connection.commit()
        return self._connection

    def close(self) -> None:
        with self._lock:
            if self._connection is not None:
                self._connection.close()
                self._connection = None

    def _execute(self, sql: str, params: tuple = ()) -> None:
        with self._lock:
            connection = self._connect()
            connection.execute(sql, params)
            connection.commit()

    def _query_one(self, sql: str, params: tuple = ()) -> Optional[dict]:
        with self._lock:
            row = self._connect().execute(sql, params).fetchone()
            return dict(row) if row else None

    def _query_all(self, sql: str, params: tuple = ()) -> list[dict]:
        with self._lock:
            rows = self._connect().execute(sql, params).fetchall()
            return [dict(row) for row in rows]

    # -- создание ключей ----------------------------------------------

    @staticmethod
    def generate_key_value() -> str:
        groups = []
        for _ in range(KEY_GROUPS):
            groups.append("".join(secrets.choice(KEY_ALPHABET) for _ in range(KEY_GROUP_LENGTH)))
        return "-".join(groups)

    def create_keys(self, days: int, count: int = 1, note: str = "") -> list[dict]:
        created: list[dict] = []
        for _ in range(count):
            for _attempt in range(MAX_GENERATION_ATTEMPTS):
                value = self.generate_key_value()
                try:
                    self._execute(
                        "INSERT INTO license_keys (key, duration_days, note, created_at)"
                        " VALUES (?, ?, ?, ?)",
                        (value, days, note, to_iso(utcnow())),
                    )
                except sqlite3.IntegrityError:
                    continue
                record = self.get_key(value)
                if record is not None:
                    created.append(record)
                break
            else:
                raise RuntimeError("Не удалось сгенерировать уникальный ключ")
        return created

    # -- чтение --------------------------------------------------------

    def get_key(self, key: str) -> Optional[dict]:
        return self._query_one(
            "SELECT * FROM license_keys WHERE key = ?", (normalize_key(key),)
        )

    def list_keys(self, limit: int = 500) -> list[dict]:
        return self._query_all(
            "SELECT * FROM license_keys ORDER BY id DESC LIMIT ?", (limit,)
        )

    # -- управление ------------------------------------------------------

    def set_revoked(self, key: str, revoked: bool) -> Optional[dict]:
        self._execute(
            "UPDATE license_keys SET revoked = ? WHERE key = ?",
            (1 if revoked else 0, normalize_key(key)),
        )
        return self.get_key(key)

    def reset_hwid(self, key: str) -> Optional[dict]:
        self._execute(
            "UPDATE license_keys SET hwid = NULL WHERE key = ?", (normalize_key(key),)
        )
        return self.get_key(key)

    def force_expire(self, key: str) -> Optional[dict]:
        """Тестовая/служебная утилита: сделать ключ истёкшим."""
        past = to_iso(utcnow() - timedelta(days=1))
        self._execute(
            "UPDATE license_keys SET activated_at = ?, expires_at = ? WHERE key = ?",
            (past, past, normalize_key(key)),
        )
        return self.get_key(key)

    # -- активация и статус ---------------------------------------------

    def _evaluate(self, record: dict, hwid: Optional[str], bind_hwid: bool) -> tuple[str, dict]:
        if record["revoked"]:
            return "revoked", record
        if not record["activated_at"]:
            return "unused", record
        if record["expires_at"] and parse_iso(record["expires_at"]) <= utcnow():
            return "expired", record
        if bind_hwid and hwid and record["hwid"] and record["hwid"] != hwid:
            return "hwid_mismatch", record
        return "active", record

    def _payload(self, record: dict, first: bool) -> dict:
        return {
            "status": "ok",
            "first": first,
            "days_left": days_left(record["expires_at"]),
            "duration_days": record["duration_days"],
            "activated_at": record["activated_at"],
            "expires_at": record["expires_at"],
        }

    def activate(self, key: str, hwid: Optional[str], bind_hwid: bool = False) -> tuple[int, dict]:
        record = self.get_key(key)
        if record is None:
            return 404, {"status": "invalid"}

        state, record = self._evaluate(record, hwid, bind_hwid)
        if state == "revoked":
            return 403, {"status": "revoked"}
        if state == "expired":
            return 403, {"status": "expired"}
        if state == "hwid_mismatch":
            return 403, {"status": "hwid_mismatch"}

        if state == "unused":
            now = utcnow()
            expires = now + timedelta(days=record["duration_days"])
            self._execute(
                "UPDATE license_keys"
                " SET activated_at = ?, expires_at = ?, hwid = ?,"
                " activations = activations + 1 WHERE key = ?",
                (to_iso(now), to_iso(expires), hwid, record["key"]),
            )
            return 200, self._payload(self.get_key(record["key"]) or record, first=True)

        # Уже активирован: после reset-hwid разрешить привязку к новому ПК.
        if hwid and not record["hwid"]:
            self._execute(
                "UPDATE license_keys SET hwid = ? WHERE key = ?", (hwid, record["key"])
            )
        return 200, self._payload(record, first=False)

    def status(self, key: str, hwid: Optional[str], bind_hwid: bool = False) -> tuple[int, dict]:
        record = self.get_key(key)
        if record is None:
            return 404, {"status": "invalid"}

        state, record = self._evaluate(record, hwid, bind_hwid)
        if state == "revoked":
            return 403, {"status": "revoked"}
        if state == "expired":
            return 403, {"status": "expired"}
        if state == "hwid_mismatch":
            return 403, {"status": "hwid_mismatch"}
        if state == "unused":
            return 200, {
                "status": "unused",
                "first": False,
                "days_left": record["duration_days"],
                "duration_days": record["duration_days"],
                "activated_at": None,
                "expires_at": None,
            }
        return 200, self._payload(record, first=False)
