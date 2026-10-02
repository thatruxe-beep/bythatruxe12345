#!/usr/bin/env python3
"""Офлайн-генератор лицензионных ключей «18:32 cheat».

Ключи подписаны HMAC-SHA256 и проверяются прямо внутри DLL — сервер,
бот и интернет не нужны.

Два вида ключей:

1. **Привязанный к ПК (рекомендуется для продажи).** Покупатель видит свой
   HWID в окне активации (10 символов) и отправляет вам; вы генерируете ключ
   под этот HWID. На любом другом компьютере такой ключ не примется:

       python license/genkeys.py 30 1 --hwid 1A2B3C4D5E

2. **Универсальный (без привязки).** Работает на любом ПК — использовать
   только для собственных тестов:

       python license/genkeys.py 7 1

Срок действия начинается с первой активации. Формат ключа:
XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX (универсальный) или
XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX (привязанный).

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
KEYS_DIR = Path(__file__).resolve().parent / "keys"


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


def parse_hwid(text: str) -> int:
    cleaned = text.strip().replace("-", "").replace(" ", "").upper()
    if len(cleaned) != 10 or any(c not in "0123456789ABCDEF" for c in cleaned):
        raise SystemExit(
            f"HWID должен быть 10 hex-символов (как в окне активации), получено: {text!r}"
        )
    return int(cleaned, 16)


def make_key(minutes: int, hwid: int | None = None) -> str:
    """hwid=None → универсальный ключ v1; иначе ключ v2, привязанный к HWID."""
    serial = secrets.randbits(32)
    if hwid is None:
        payload = (
            bytes([1])
            + minutes.to_bytes(4, "little")
            + serial.to_bytes(4, "little")
        )
    else:
        payload = (
            bytes([2])
            + minutes.to_bytes(4, "little")
            + serial.to_bytes(4, "little")
            + hwid.to_bytes(5, "little")
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
    parser = argparse.ArgumentParser(
        description="Генератор ключей 18:32 cheat",
        epilog=(
            "Примеры:\n"
            "  python license/genkeys.py 30 1 --hwid 1A2B3C4D5E   ключ на 30 дней для конкретного ПК\n"
            "  python license/genkeys.py 365 1 --hwid 1A2B3C4D5E  годовой ключ для конкретного ПК\n"
            "  python license/genkeys.py 5 1 --minutes             5-минутный тестовый (универсальный)\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("days", nargs="?", type=int, help="срок в днях (или минутах с --minutes)")
    parser.add_argument("count", nargs="?", type=int, default=1, help="сколько ключей (по умолчанию 1)")
    parser.add_argument("--minutes", action="store_true", help="считать срок в минутах, а не днях")
    parser.add_argument(
        "--hwid",
        default=None,
        help="HWID покупателя (10 hex-символов из окна активации) — ключ будет работать только на этом ПК",
    )
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

    hwid = parse_hwid(args.hwid) if args.hwid else None

    if args.days is not None:
        if args.days < 1:
            raise SystemExit("Срок должен быть положительным.")
        count = max(1, min(args.count, 1000))
        unit = "минут" if args.minutes else days_word(args.days)
        minutes = args.days if args.minutes else args.days * 1440
        keys = [make_key(minutes, hwid) for _ in range(count)]

        if hwid is not None:
            path = out_dir / f"keys_{args.days}{'m' if args.minutes else 'd'}_hwid_{hwid:010X}.txt"
            label = f"{args.days} {unit}, HWID {hwid:010X}"
        else:
            suffix = "minutes" if args.minutes else "days"
            path = out_dir / f"keys_{args.days}_{suffix}.txt"
            label = f"{args.days} {unit}"
            print("[gen] ВНИМАНИЕ: ключ без --hwid универсальный — его можно передать другому ПК.")
            print("[gen] Для продажи генерируйте с --hwid покупателя.")
        write_batch(path, keys, label)
        return

    # Стандартный набор (универсальные ключи, без привязки).
    print("[gen] Стандартный набор — универсальные ключи (для тестов).")
    print("[gen] Ключи для покупателей: genkeys.py <дней> 1 --hwid <HWID покупателя>.")
    for days, count, name, *rest in [
        (30, 100, "keys_30_days.txt"),
        (180, 100, "keys_180_days.txt"),
        (365, 100, "keys_365_days.txt"),
        (5, 15, "keys_5_minutes.txt", True),
    ]:
        in_minutes = bool(rest and rest[0])
        minutes = days if in_minutes else days * 1440
        label = f"{days} минут" if in_minutes else f"{days} {days_word(days)}"
        keys = [make_key(minutes) for _ in range(count)]
        write_batch(out_dir / name, keys, label)

    print("[gen] готово. Ключи выдавайте по одному покупателю.")


if __name__ == "__main__":
    main()
