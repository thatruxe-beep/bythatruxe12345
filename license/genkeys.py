#!/usr/bin/env python3
"""Офлайн-генератор лицензионных ключей «18:32 cheat».

Все ключи привязываются к HWID конкретного компьютера — универсальных
ключей нет: ключ, выданный покупателю, нельзя передать другому человеку.

Как пользоваться:
1. Покупатель запускает игру с читом — в правом верхнем углу окна
   активации виден его HWID (10 символов) и отправляет его вам.
2. Вы генерируете ключ под этот HWID:

       python license/genkeys.py 30 1 --hwid 1A2B3C4D5E    # 30 дней
       python license/genkeys.py 365 1 --hwid 1A2B3C4D5E   # год
       python license/genkeys.py 5 1 --hwid 1A2B3C4D5E --minutes  # 5 минут

3. Отправляете ключ покупателю. Срок начнётся с первой активации.

Формат ключа: XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX.

ВНИМАНИЕ: секрет задан и в этом скрипте, и в DLL (Core/License.cpp).
Если сменить секрет — все выданные ключи перестанут работать.
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


def make_key(minutes: int, hwid: int) -> str:
    payload = (
        bytes([2])
        + minutes.to_bytes(4, "little")
        + secrets.randbits(32).to_bytes(4, "little")
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


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Генератор ключей 18:32 cheat (только с привязкой к HWID)",
        epilog=(
            "Примеры:\n"
            "  python license/genkeys.py 30 1 --hwid 1A2B3C4D5E   ключ на 30 дней\n"
            "  python license/genkeys.py 365 3 --hwid 1A2B3C4D5E  три годовых ключа\n"
            "  python license/genkeys.py 5 1 --hwid 1A2B3C4D5E --minutes   5 минут (тест)\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("days", nargs="?", type=int, help="срок в днях (или минутах с --minutes)")
    parser.add_argument("count", nargs="?", type=int, default=1, help="сколько ключей (по умолчанию 1)")
    parser.add_argument("--minutes", action="store_true", help="считать срок в минутах, а не днях")
    parser.add_argument(
        "--hwid",
        required=True,
        help="HWID покупателя (10 hex-символов из окна активации)",
    )
    parser.add_argument("--out", default=str(KEYS_DIR), help="куда сохранять файлы")
    parser.add_argument("--list", action="store_true", help="показать уже сгенерированные файлы")
    args = parser.parse_args()

    out_dir = Path(args.out)
    hwid = parse_hwid(args.hwid)

    if args.list:
        if not out_dir.is_dir():
            print("Файлов ключей ещё нет.")
            return
        for path in sorted(out_dir.glob("*.txt")):
            lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
            print(f"{path.name}: {len(lines)} ключей")
        return

    if args.days is None:
        raise SystemExit(
            "Укажите срок: python license/genkeys.py <дней> [сколько] --hwid <HWID>\n"
            "Например: python license/genkeys.py 30 1 --hwid 1A2B3C4D5E"
        )
    if args.days < 1:
        raise SystemExit("Срок должен быть положительным.")

    count = max(1, min(args.count, 1000))
    unit = "минут" if args.minutes else days_word(args.days)
    minutes = args.days if args.minutes else args.days * 1440
    keys = [make_key(minutes, hwid) for _ in range(count)]

    suffix = "m" if args.minutes else "d"
    path = out_dir / f"keys_{args.days}{suffix}_hwid_{hwid:010X}.txt"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(keys) + "\n", encoding="utf-8")
    print(f"[gen] {path.name}: {len(keys)} ключей — {args.days} {unit}, HWID {hwid:010X}")


if __name__ == "__main__":
    main()
