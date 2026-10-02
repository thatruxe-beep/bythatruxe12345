"""Транспорт Telegram Bot API: long polling поверх httpx, без внешних библиотек."""

from __future__ import annotations

import time

import httpx

from .handlers import BotHandlers

API_BASE = "https://api.telegram.org"
POLL_TIMEOUT_SECONDS = 25
MESSAGE_CHUNK = 3500


class TelegramTransport:
    def __init__(self, token: str, handlers: BotHandlers) -> None:
        self.token = token
        self.handlers = handlers
        self.http = httpx.Client(timeout=POLL_TIMEOUT_SECONDS + 15.0)

    def _call(self, method: str, payload: dict) -> list | dict | None:
        try:
            response = self.http.post(f"{API_BASE}/bot{self.token}/{method}", json=payload)
            response.raise_for_status()
            data = response.json()
        except httpx.HTTPError:
            return None
        if not data.get("ok"):
            return None
        return data.get("result")

    def send(self, chat_id: int, text: str) -> None:
        for start in range(0, len(text), MESSAGE_CHUNK):
            self._call(
                "sendMessage",
                {"chat_id": chat_id, "text": text[start : start + MESSAGE_CHUNK]},
            )

    def run(self) -> None:
        print("[bot] запущен, жду сообщений…")
        offset = 0
        while True:
            updates = self._call(
                "getUpdates",
                {
                    "timeout": POLL_TIMEOUT_SECONDS,
                    "offset": offset,
                    "allowed_updates": ["message"],
                },
            )
            if updates is None:
                time.sleep(3.0)
                continue
            for update in updates:
                offset = max(offset, update["update_id"] + 1)
                message = update.get("message")
                if not message:
                    continue
                user_id = (message.get("from") or {}).get("id", 0)
                chat_id = (message.get("chat") or {}).get("id", user_id)
                for reply in self.handlers.handle(user_id, message.get("text", "")):
                    self.send(chat_id, reply)

    def close(self) -> None:
        self.http.close()
