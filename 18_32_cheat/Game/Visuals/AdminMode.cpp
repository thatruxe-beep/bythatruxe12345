#include "Game/Features.h"

#include "Game/Visuals/AdminMode.hpp"

// Админ мод: при включении чит сам прописывает команду "adminmode" в консоль
// MTA (F8) и нажимает Enter, при выключении — повторяет её (команда-переключатель).
// Ввод идёт через SendInput: синтетические события попадают в системный поток
// ввода и влияют на GetAsyncKeyState, поэтому их видит и MTA.

namespace
{
    constexpr unsigned int kConsoleVk = VK_F8;

    // Посылка одного нажатия виртуальной клавиши.
    void SendVk(WORD vk, bool down)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;

        if (!down)
        {
            input.ki.dwFlags = KEYEVENTF_KEYUP;
        }

        SendInput(1, &input, sizeof(INPUT));
    }

    // Последовательность ввода, растянутая по времени (кадры Present), чтобы
    // не замораживать рендер и дать консоли MTA успеть открыться и принять
    // фокус перед печатью.
    struct ConsoleSequence
    {
        WORD keys[40]{};
        unsigned int holdMs[40]{};
        unsigned int gapMs[40]{};
        unsigned int count = 0;
        unsigned int pos = 0;
        bool keyDown = false;
        unsigned int nextActionTick = 0;
        bool active = false;
    };

    ConsoleSequence& Sequence()
    {
        static ConsoleSequence seq;
        return seq;
    }

    void QueueKey(WORD vk, unsigned int holdMs, unsigned int gapMs)
    {
        ConsoleSequence& seq = Sequence();

        if (seq.count < 40)
        {
            seq.keys[seq.count] = vk;
            seq.holdMs[seq.count] = holdMs;
            seq.gapMs[seq.count] = gapMs;
            ++seq.count;
        }
    }

    void StartCommand(const char* command)
    {
        ConsoleSequence& seq = Sequence();
        seq.count = 0;
        seq.pos = 0;
        seq.keyDown = false;
        seq.nextActionTick = GetTickCount() + 150;
        seq.active = true;

        // Открыть консоль и дать ей время получить фокус ввода.
        QueueKey(kConsoleVk, 30, 350);

        // Сама команда (регистр для консоли не важен).
        for (const char* c = command; *c != '\0'; ++c)
        {
            char ch = *c;

            if (ch >= 'a' && ch <= 'z')
            {
                ch = static_cast<char>(ch - 'a' + 'A');
            }

            if (ch >= 'A' && ch <= 'Z')
            {
                QueueKey(static_cast<WORD>(ch), 20, 30);
            }
            else if (ch >= '0' && ch <= '9')
            {
                QueueKey(static_cast<WORD>(ch), 20, 30);
            }
        }

        // Выполнить и закрыть консоль.
        QueueKey(VK_RETURN, 30, 200);
        QueueKey(kConsoleVk, 30, 30);
    }

    void PumpSequence()
    {
        ConsoleSequence& seq = Sequence();

        if (!seq.active)
        {
            return;
        }

        if (GetTickCount() < seq.nextActionTick)
        {
            return;
        }

        if (!seq.keyDown)
        {
            if (seq.pos >= seq.count)
            {
                seq.active = false;
                return;
            }

            SendVk(seq.keys[seq.pos], true);
            seq.keyDown = true;
            seq.nextActionTick = GetTickCount() + seq.holdMs[seq.pos];
        }
        else
        {
            SendVk(seq.keys[seq.pos], false);
            seq.keyDown = false;
            seq.nextActionTick = GetTickCount() + seq.gapMs[seq.pos];
            ++seq.pos;
        }
    }
}

void AdminMode::Update()
{
    PumpSequence();

    static bool wasOn = false;
    const bool on = g_cfg.adminmode;

    if (on != wasOn)
    {
        // adminmode — команда-переключатель: одна и та же строка включает
        // и выключает режим, поэтому просто прописываем её снова.
        StartCommand("adminmode");
        wasOn = on;
    }
}
