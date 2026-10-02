#include "Hwid.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

std::string GetMachineId()
{
    DWORD volumeSerial = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, nullptr, &volumeSerial, nullptr, nullptr, 0);

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 2] = {};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(computer, &size);

    uint64_t hash = 1469598103934665603ull; // FNV-1a 64
    auto mix = [&hash](const void* data, size_t length) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < length; ++i)
        {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
    };
    mix(&volumeSerial, sizeof(volumeSerial));
    mix(computer, size * sizeof(wchar_t));

    static const char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(16);
    for (int shift = 60; shift >= 0; shift -= 4)
        result += digits[(hash >> shift) & 0xF];
    return result;
}
