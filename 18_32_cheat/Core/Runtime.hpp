#pragma once

#include <windows.h>

namespace Runtime
{
    void Initialize(HMODULE module);
    void RequestUnload();
    bool IsRunning();
    HMODULE GetModule();
}
