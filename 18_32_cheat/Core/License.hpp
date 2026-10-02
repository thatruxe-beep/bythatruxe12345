#pragma once

// Автономная проверка лицензионных ключей «18:32 cheat».
// Портативная часть без Win32: SHA-256, HMAC, base32, формат ключа и
// сериализация состояния активации. Срок ключа начинается с первой
// активации и никогда не продлевается автоматически.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace license
{
    constexpr size_t kKeyCharCount = 32; // 8 групп по 4 символа
    constexpr size_t kKeyDataBytes = 20; // 9 байт данных + 11 байт подписи
    constexpr size_t kPayloadBytes = 9;  // версия + срок + серийник
    constexpr size_t kSignatureBytes = 11;
    constexpr uint8_t kKeyVersion = 1;
    constexpr size_t kMaxRecords = 64; // максимум активированных ключей в истории

    const char* Base32Alphabet()
    {
        return "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    }

    // ---------------------------------------------------------------- SHA-256

    namespace detail
    {
        // Раундовые константы SHA-2 (FIPS 180-4).
        inline constexpr uint32_t kSha256K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
        };

        class Sha256
        {
        public:
            Sha256()
            {
                Reset();
            }

            void Reset()
            {
                datalen_ = 0;
                bitlen_ = 0;
                h_[0] = 0x6a09e667u; h_[1] = 0xbb67ae85u; h_[2] = 0x3c6ef372u; h_[3] = 0xa54ff53au;
                h_[4] = 0x510e527fu; h_[5] = 0x9b05688cu; h_[6] = 0x1f83d9abu; h_[7] = 0x5be0cd19u;
            }

            void Update(const uint8_t* data, size_t length)
            {
                for (size_t i = 0; i < length; ++i)
                {
                    data_[datalen_] = data[i];
                    ++datalen_;
                    if (datalen_ == 64)
                    {
                        Transform();
                        bitlen_ += 512;
                        datalen_ = 0;
                    }
                }
            }

            void Final(uint8_t digest[32])
            {
                size_t i = datalen_;

                if (datalen_ < 56)
                {
                    data_[i++] = 0x80;
                    while (i < 56)
                        data_[i++] = 0x00;
                }
                else
                {
                    data_[i++] = 0x80;
                    while (i < 64)
                        data_[i++] = 0x00;
                    Transform();
                    for (i = 0; i < 56; ++i)
                        data_[i] = 0x00;
                }

                bitlen_ += datalen_ * 8;
                data_[63] = static_cast<uint8_t>(bitlen_);
                data_[62] = static_cast<uint8_t>(bitlen_ >> 8);
                data_[61] = static_cast<uint8_t>(bitlen_ >> 16);
                data_[60] = static_cast<uint8_t>(bitlen_ >> 24);
                data_[59] = static_cast<uint8_t>(bitlen_ >> 32);
                data_[58] = static_cast<uint8_t>(bitlen_ >> 40);
                data_[57] = static_cast<uint8_t>(bitlen_ >> 48);
                data_[56] = static_cast<uint8_t>(bitlen_ >> 56);
                Transform();

                for (int j = 0; j < 8; ++j)
                {
                    digest[j * 4 + 0] = static_cast<uint8_t>(h_[j] >> 24);
                    digest[j * 4 + 1] = static_cast<uint8_t>(h_[j] >> 16);
                    digest[j * 4 + 2] = static_cast<uint8_t>(h_[j] >> 8);
                    digest[j * 4 + 3] = static_cast<uint8_t>(h_[j]);
                }
            }

        private:
            void Transform()
            {
                uint32_t m[64];
                for (int i = 0; i < 16; ++i)
                {
                    m[i] = (static_cast<uint32_t>(data_[i * 4 + 0]) << 24)
                         | (static_cast<uint32_t>(data_[i * 4 + 1]) << 16)
                         | (static_cast<uint32_t>(data_[i * 4 + 2]) << 8)
                         | static_cast<uint32_t>(data_[i * 4 + 3]);
                }
                for (int i = 16; i < 64; ++i)
                {
                    const uint32_t s0 = Rotr(m[i - 15], 7) ^ Rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
                    const uint32_t s1 = Rotr(m[i - 2], 17) ^ Rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
                    m[i] = m[i - 16] + s0 + m[i - 7] + s1;
                }

                uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
                uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];

                for (int i = 0; i < 64; ++i)
                {
                    const uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
                    const uint32_t ch = (e & f) ^ (~e & g);
                    const uint32_t temp1 = h + S1 + ch + kSha256K[i] + m[i];
                    const uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
                    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                    const uint32_t temp2 = S0 + maj;

                    h = g; g = f; f = e; e = d + temp1;
                    d = c; c = b; b = a; a = temp1 + temp2;
                }

                h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
                h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
            }

            static uint32_t Rotr(uint32_t value, uint32_t bits)
            {
                return (value >> bits) | (value << (32 - bits));
            }

            uint32_t h_[8];
            uint8_t data_[64];
            uint32_t datalen_;
            uint64_t bitlen_;
        };

    }

    inline void HmacSha256(const uint8_t* key, size_t key_length,
                           const uint8_t* message, size_t message_length,
                           uint8_t out[32])
    {
        uint8_t key_block[64];
        std::memset(key_block, 0, sizeof(key_block));

        if (key_length > 64)
        {
            detail::Sha256 hasher;
            hasher.Update(key, key_length);
            hasher.Final(key_block);
        }
        else
        {
            std::memcpy(key_block, key, key_length);
        }

        uint8_t ipad[64];
        uint8_t opad[64];
        for (int i = 0; i < 64; ++i)
        {
            ipad[i] = key_block[i] ^ 0x36;
            opad[i] = key_block[i] ^ 0x5c;
        }

        uint8_t inner[32];
        detail::Sha256 hasher;
        hasher.Update(ipad, sizeof(ipad));
        hasher.Update(message, message_length);
        hasher.Final(inner);

        detail::Sha256 outer;
        outer.Update(opad, sizeof(opad));
        outer.Update(inner, sizeof(inner));
        outer.Final(out);
    }

    // --------------------------------------------------------------- base32

    inline std::string Base32Encode(const uint8_t* data, size_t length)
    {
        const char* alphabet = Base32Alphabet();
        std::string out;
        out.reserve((length * 8 + 4) / 5);

        uint32_t buffer = 0;
        int bits = 0;
        for (size_t i = 0; i < length; ++i)
        {
            buffer = (buffer << 8) | data[i];
            bits += 8;
            while (bits >= 5)
            {
                bits -= 5;
                out += alphabet[(buffer >> bits) & 31];
            }
        }
        if (bits > 0)
            out += alphabet[(buffer << (5 - bits)) & 31];
        return out;
    }

    inline bool Base32Decode(const std::string& text, uint8_t* out, size_t out_length)
    {
        if (text.size() != (out_length * 8 + 4) / 5)
            return false;

        const char* alphabet = Base32Alphabet();
        uint32_t buffer = 0;
        int bits = 0;
        size_t written = 0;

        for (size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\0')
                return false;
            const char* position = std::strchr(alphabet, c);
            if (position == nullptr)
                return false;

            buffer = (buffer << 5) | static_cast<uint32_t>(position - alphabet);
            bits += 5;
            if (bits >= 8)
            {
                bits -= 8;
                if (written >= out_length)
                    return false;
                out[written] = static_cast<uint8_t>((buffer >> bits) & 0xFF);
                ++written;
            }
        }
        return written == out_length;
    }

    // ---------------------------------------------------------------- ключи

    struct KeyInfo
    {
        bool valid = false;
        uint32_t minutes = 0; // срок действия в минутах
        uint32_t serial = 0;  // уникальный номер ключа
    };

    inline std::string NormalizeKeyText(const std::string& user_text)
    {
        std::string out;
        out.reserve(user_text.size());
        for (size_t i = 0; i < user_text.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(user_text[i]);
            if (c >= 'a' && c <= 'z')
                out += static_cast<char>(c - 'a' + 'A');
            else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                out += static_cast<char>(c);
        }
        return out;
    }

    inline bool ParseKey(const std::string& normalized, const uint8_t secret[32], KeyInfo& out)
    {
        out = KeyInfo();
        if (normalized.size() != kKeyCharCount)
            return false;

        uint8_t data[kKeyDataBytes];
        if (!Base32Decode(normalized, data, kKeyDataBytes))
            return false;
        if (data[0] != kKeyVersion)
            return false;

        uint8_t mac[32];
        HmacSha256(secret, 32, data, kPayloadBytes, mac);
        if (std::memcmp(mac, data + kPayloadBytes, kSignatureBytes) != 0)
            return false;

        out.minutes = static_cast<uint32_t>(data[1])
                    | (static_cast<uint32_t>(data[2]) << 8)
                    | (static_cast<uint32_t>(data[3]) << 16)
                    | (static_cast<uint32_t>(data[4]) << 24);
        out.serial = static_cast<uint32_t>(data[5])
                   | (static_cast<uint32_t>(data[6]) << 8)
                   | (static_cast<uint32_t>(data[7]) << 16)
                   | (static_cast<uint32_t>(data[8]) << 24);
        out.valid = true;
        return true;
    }

    // -------------------------------------------------- состояние активации

    struct ActivationRecord
    {
        uint32_t serial = 0;
        int64_t activation_unix = 0;
    };

    struct State
    {
        std::vector<ActivationRecord> records; // история: срок не сбрасывается
        std::string current_key;               // 32 символа или пусто
        int64_t last_seen_unix = 0;
    };

    inline void BytesToHex(const uint8_t* data, size_t length, char* out, size_t out_size)
    {
        static const char digits[] = "0123456789abcdef";
        size_t written = 0;
        for (size_t i = 0; i < length && written + 2 < out_size; ++i)
        {
            out[written++] = digits[data[i] >> 4];
            out[written++] = digits[data[i] & 0xF];
        }
        out[written] = '\0';
    }

    inline std::string SerializeState(const State& state, const uint8_t secret[32])
    {
        std::string payload = "1832L1;";
        payload += state.current_key.empty() ? "-" : state.current_key;
        payload += ";";
        payload += std::to_string(state.last_seen_unix);
        payload += ";";
        payload += std::to_string(state.records.size());

        for (const ActivationRecord& record : state.records)
        {
            payload += ";";
            payload += std::to_string(record.serial);
            payload += ":";
            payload += std::to_string(record.activation_unix);
        }

        uint8_t mac[32];
        HmacSha256(secret, 32, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), mac);
        char hex[65];
        BytesToHex(mac, 32, hex, sizeof(hex));

        return payload + "|" + hex;
    }

    inline bool DeserializeState(const std::string& blob, const uint8_t secret[32], State& out)
    {
        out = State();

        const size_t split = blob.rfind('|');
        if (split == std::string::npos)
            return false;

        const std::string payload = blob.substr(0, split);
        const std::string hex = blob.substr(split + 1);

        uint8_t mac[32];
        HmacSha256(secret, 32, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), mac);
        char expected[65];
        BytesToHex(mac, 32, expected, sizeof(expected));
        if (hex != expected)
            return false;

        std::vector<std::string> parts;
        size_t start = 0;
        while (true)
        {
            const size_t comma = payload.find(';', start);
            if (comma == std::string::npos)
            {
                parts.push_back(payload.substr(start));
                break;
            }
            parts.push_back(payload.substr(start, comma - start));
            start = comma + 1;
        }

        if (parts.size() < 4 || parts[0] != "1832L1")
            return false;

        out.current_key = (parts[1] == "-") ? std::string() : parts[1];
        out.last_seen_unix = strtoll(parts[2].c_str(), nullptr, 10);

        const size_t count = static_cast<size_t>(atoi(parts[3].c_str()));
        if (count > kMaxRecords || parts.size() < 4 + count)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            const std::string& record = parts[4 + i];
            const size_t colon = record.find(':');
            if (colon == std::string::npos)
                return false;
            ActivationRecord entry;
            entry.serial = static_cast<uint32_t>(strtoul(record.substr(0, colon).c_str(), nullptr, 10));
            entry.activation_unix = strtoll(record.substr(colon + 1).c_str(), nullptr, 10);
            out.records.push_back(entry);
        }
        return true;
    }
}

// --------------------------------------------------------------------------
// Runtime-API (реализация — Core/License.cpp, Win32).
//
// Жизненный цикл: Initialize() при загрузке DLL → если ключ сохранён и жив,
// функции сразу работают; иначе рисуется окно активации (Menu/Auth.cpp).
// SubmitKey() проверяет ключ: неверный → игра закрывается; верный → срок
// фиксируется с первой активации и больше не продлевается.
namespace License
{
    enum class Phase
    {
        NeedKey,    // ключ ещё не введён
        Authorized, // подписка действует
        Expired,    // срок вышел — нужен новый ключ
        WrongKey,   // введён неверный ключ — игра закроется
        Blocked,    // перевод системного времени — игра закроется
    };

    void Initialize();                 // вызвать один раз при старте
    Phase GetPhase();
    bool Authorized();                 // true → функции чита разрешены
    bool NeedsInputOverlay();          // true → рисовать окно активации
    char* KeyBuffer();                 // буфер ввода ключа (64 байта)
    void SubmitKey();                  // проверить ключ из буфера
    void Tick();                       // периодическая проверка (каждый кадр)
    int64_t RemainingSeconds();
    void FormatRemaining(char* out, size_t size); // «29 дн. 04:12» / «04:59»
    void FormatExpiry(char* out, size_t size);    // «01.11.2026 17:30»
    void FormattedKey(char* out, size_t size);    // «XXXX-XXXX-…-XXXX»
    int SecondsToExit();               // -1 — таймер не запущен
    void EnforceExit();                // закрыть игру, если таймер истёк
}
