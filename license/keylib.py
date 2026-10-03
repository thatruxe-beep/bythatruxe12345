"""Общие операции с ключами «18:32 cheat»: генерация и проверка подписи.

Используется и генератором (genkeys.py), и лицензионным сервером
(автоматическая регистрация заранее сгенерированных ключей).
"""

from __future__ import annotations

import hashlib
import hmac
import secrets

SECRET = bytes.fromhex("e2387d0a0d4f550073695ab0356350f0b9122e2aa7f84c27bfc4a8e5ab4405bb")
ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
SIGNATURE_BYTES = 11


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


def base32_decode(text: str, length: int) -> bytes | None:
    if len(text) != (length * 8 + 4) // 5:
        return None
    value = 0
    bits = 0
    out = bytearray()
    for ch in text:
        index = ALPHABET.find(ch)
        if index < 0:
            return None
        value = (value << 5) | index
        bits += 5
        if bits >= 8:
            bits -= 8
            out.append((value >> bits) & 0xFF)
    return bytes(out) if len(out) == length else None


def normalize_key(text: str) -> str:
    return "".join(ch for ch in text.strip().upper() if ch.isalnum())


def make_key(minutes: int, hwid: int | None = None) -> str:
    """hwid=None — универсальный ключ (привязка произойдёт при первой
    активации на сервере); иначе ключ сразу привязан к этому HWID."""
    serial = secrets.randbits(32)
    if hwid is None:
        payload = bytes([1]) + minutes.to_bytes(4, "little") + serial.to_bytes(4, "little")
    else:
        payload = (
            bytes([2])
            + minutes.to_bytes(4, "little")
            + serial.to_bytes(4, "little")
            + hwid.to_bytes(5, "little")
        )
    mac = hmac.new(SECRET, payload, hashlib.sha256).digest()[:SIGNATURE_BYTES]
    raw = base32_encode(payload + mac)
    return "-".join(raw[i : i + 4] for i in range(0, len(raw), 4))


def verify_key(text: str) -> dict | None:
    """Проверяет подпись ключа. Возвращает словарь с минутами, серийником и
    (для v2) HWID, либо None, если ключ не подписан нашим секретом."""
    normalized = normalize_key(text)
    if len(normalized) == 32:
        data_length, version = 20, 1
    elif len(normalized) == 40:
        data_length, version = 25, 2
    else:
        return None

    data = base32_decode(normalized, data_length)
    if data is None or data[0] != version:
        return None

    payload_length = data_length - SIGNATURE_BYTES
    expected = hmac.new(SECRET, data[:payload_length], hashlib.sha256).digest()[:SIGNATURE_BYTES]
    if not hmac.compare_digest(expected, data[payload_length:]):
        return None

    minutes = int.from_bytes(data[1:5], "little")
    serial = int.from_bytes(data[5:9], "little")
    if minutes < 1:
        return None

    result = {"minutes": minutes, "serial": serial, "hwid": None}
    if version == 2:
        result["hwid"] = int.from_bytes(data[9:14], "little")
    return result
