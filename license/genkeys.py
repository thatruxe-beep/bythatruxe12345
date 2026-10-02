#!/usr/bin/env python3
"""Офлайн-генератор лицензионных ключей «18:32 cheat».

Ключи подписаны HMAC-SHA256 и проверяются прямо внутри DLL — сервер,
бот и интернет не нужны. Формат: XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX.
Срок действия начинается с первой активации в лоадере/DLL.

Использование:
    python license/genkeys.py                      # стандартный набор: 30/180/365 дней и 5 минут
    python license/genkeys.py 60 10                # 10 ключей на 60 дней
    python license/genkeys.py 43200 3 --minutes    # 3 ключа на 43200 МИНУТ
    python license/genkeys.py --list               # показать файлы ключей

ВНИМАНИЕ: секрет задан и в этом скрипте, и в DLL (Core/License.cpp).
Если сменить секрет — старые ключи перестанут работать, а все
сгенерированные файлы нужно перегенерировать.
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import secrets
from pathlib import Path

SECRET = bytes.fromhex("e2387d0a0d4f550073695ab0356350f0b9122e2aa7f84c27bfc4a8e5ab4405bb")
ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
KEY_VERSION = 1
KEYS_DIR = Path(__file__).resolve().parent / "keys"

# Стандартный заказ: (дней, количество, имя файла)
DEFAULT_BATCH = [
    (30, 100, "keys_30_days.txt"),
    (180, 100, "keys_180_days.txt"),
    (365, 100, "keys_365_days.txt"),
    (5, 15, "keys_5_minutes.txt", True),  # True — срок в минутах, а не днях
]


def base32_encode(data: bytes) -> str:
    value = 0
    bits = 0
    out = []
    for byte in data:
        value = (value << 8) | byte
        bits += 8
        while bits >= 5:
            bits -= 5
            out.append(ALPHABET[(value >> bits) & 31])
    if bits:
        out.append(ALPHABET[(value << (5 - bits)) & 31])
    return "".join(out)


def make_key(minutes: int) -> str:
    payload = (
        bytes([KEY_VERSION])
        + minutes.to_bytes(4, "little")
        + secrets.randbits(32).to_bytes(4, "little")
    )
    mac = hmac.new(SECRET, payload, hashlib.sha256).digest()[:11]
    raw = base32_encode(payload + mac)
    return "-".join(raw[i : i + 4] for i in range(0, len(raw), 4))


def days_word(days: int) -> str:
    if days % 10 == 1 and days % 100 != 11:
        return "день"
    if 2 <= days % 10 <= 4 and (days % 100 < 10 or days % 100 >= 20):
        return "дня"
    return "дней"


def write_batch(path: Path, keys: list[str], label: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(keys) + "\n", encoding="utf-8")
    print(f"[gen] {path.name}: {len(keys)} ключей — {label}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Генератор ключей 18:32 cheat")
    parser.add_argument("days", nargs="?", type=int, help="срок в днях (или минутах с --minutes)")
    parser.add_argument("count", nargs="?", type=int, default=1, help="сколько ключей (по умолчанию 1)")
    parser.add_argument("--minutes", action="store_true", help="считать срок в минутах, а не днях")
    parser.add_argument("--out", default=str(KEYS_DIR), help="куда сохранять файлы")
    parser.add_argument("--list", action="store_true", help="показать уже сгенерированные файлы")
    args = parser.parse_args()

    out_dir = Path(args.out)

    if args.list:
        if not out_dir.is_dir():
            print("Файлов ключей ещё нет.")
            return
        for path in sorted(out_dir.glob("*.txt")):
            lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
            print(f"{path.name}: {len(lines)} ключей")
        return

    if args.days is not None:
        if args.days < 1:
            raise SystemExit("Срок должен быть положительным.")
        count = max(1, min(args.count, 1000))
        unit = "минут" if args.minutes else days_word(args.days)
        minutes = args.days if args.minutes else args.days * 1440
        keys = [make_key(minutes) for _ in range(count)]
        suffix = "minutes" if args.minutes else "days"
        path = out_dir / f"keys_{args.days}_{suffix}.txt"
        write_batch(path, keys, f"{args.days} {unit}")
        return

    # Стандартный набор.
    for batch in DEFAULT_BATCH:
        days, count, name = batch[0], batch[1], batch[2]
        in_minutes = len(batch) > 3 and batch[3]
        minutes = days if in_minutes else days * 1440
        label = f"{days} минут" if in_minutes else f"{days} {days_word(days)}"
        keys = [make_key(minutes) for _ in range(count)]
        write_batch(out_dir / name, keys, label)

    print("[gen] готово. Ключи выдавайте по одному покупателю.")


if __name__ == "__main__":
    main()
