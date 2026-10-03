#!/usr/bin/env python3
"""Офлайн-генератор лицензионных ключей «18:32 cheat».

Ключи генерируются заранее, без знания HWID покупателя (как сток).
Привязка к компьютеру происходит автоматически **при первой активации**:
сервер лицензий запоминает HWID первого ПК, и на любом другом компьютере
этот же ключ работать не будет.

Использование:
    python license/genkeys.py                # сток: 100×30 дн, 100×180, 100×365, 15×5 мин
    python license/genkeys.py 60 10          # 10 ключей на 60 дней
    python license/genkeys.py 5 3 --minutes  # 3 ключа на 5 минут (тесты)
    python license/genkeys.py --list         # показать сгенерированные файлы

Ключ с привязкой к конкретному HWID при выдаче (по желанию):
    python license/genkeys.py 30 1 --hwid 1A2B3C4D5E

ВНИМАНИЕ: секрет задан и здесь, и в DLL (Core/License.cpp), и на сервере
лицензий. Смена секрета = все выданные ключи перестают работать.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from keylib import make_key

KEYS_DIR = Path(__file__).resolve().parent / "keys"

# Стандартный сток: (срок, количество, имя файла, срок_в_минутах)
DEFAULT_BATCH = [
    (30, 100, "keys_30_days.txt", False),
    (180, 100, "keys_180_days.txt", False),
    (365, 100, "keys_365_days.txt", False),
    (5, 15, "keys_5_minutes.txt", True),
]


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


def parse_hwid(text: str) -> int:
    cleaned = text.strip().replace("-", "").replace(" ", "").upper()
    if len(cleaned) != 10 or any(c not in "0123456789ABCDEF" for c in cleaned):
        raise SystemExit(
            f"HWID должен быть 10 hex-символов (как в окне активации), получено: {text!r}"
        )
    return int(cleaned, 16)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Генератор ключей 18:32 cheat",
        epilog=(
            "Примеры:\n"
            "  python license/genkeys.py               стандартный сток\n"
            "  python license/genkeys.py 60 10         10 ключей на 60 дней\n"
            "  python license/genkeys.py 5 3 --minutes 3 тестовых ключа на 5 минут\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("days", nargs="?", type=int, help="срок в днях (или минутах с --minutes)")
    parser.add_argument("count", nargs="?", type=int, default=1, help="сколько ключей (по умолчанию 1)")
    parser.add_argument("--minutes", action="store_true", help="считать срок в минутах, а не днях")
    parser.add_argument(
        "--hwid",
        default=None,
        help="необязательно: сразу привязать ключ к HWID (10 hex-символов)",
    )
    parser.add_argument("--out", default=str(KEYS_DIR), help="куда сохранять файлы")
    parser.add_argument("--list", action="store_true", help="показать уже сгенерированные файлы")
    args = parser.parse_args()

    out_dir = Path(args.out)
    hwid = parse_hwid(args.hwid) if args.hwid else None

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
        keys = [make_key(minutes, hwid) for _ in range(count)]

        if hwid is not None:
            path = out_dir / f"keys_{args.days}{'m' if args.minutes else 'd'}_hwid_{hwid:010X}.txt"
            label = f"{args.days} {unit}, HWID {hwid:010X}"
        else:
            suffix = "minutes" if args.minutes else "days"
            path = out_dir / f"keys_{args.days}_{suffix}.txt"
            label = f"{args.days} {unit}"
        write_batch(path, keys, label)
        print("[gen] Напоминание: срок начинается с первой активации;")
        print("[gen] после неё ключ привязан к ПК покупателя автоматически.")
        return

    # Стандартный сток (универсальные ключи).
    for days, count, name, in_minutes in DEFAULT_BATCH:
        minutes = days if in_minutes else days * 1440
        label = f"{days} минут" if in_minutes else f"{days} {days_word(days)}"
        keys = [make_key(minutes) for _ in range(count)]
        write_batch(out_dir / name, keys, label)

    print("[gen] готово. Привязка к ПК происходит при первой активации (сервер).")


if __name__ == "__main__":
    main()
