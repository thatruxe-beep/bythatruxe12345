#pragma once

#include <windows.h>

class DoubleJump
{
public:
    static void HandleKeyMessage(UINT message, WPARAM key);
    static void Run();
};
