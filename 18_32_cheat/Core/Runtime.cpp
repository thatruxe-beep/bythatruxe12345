#include "Core/Runtime.hpp"

#include <atomic>

namespace
{
    std::atomic_bool g_running{ false };
    HMODULE g_module = nullptr;
}

void Runtime::Initialize(HMODULE module)
{
    g_module = module;
    g_running.store(true, std::memory_order_release);
}

void Runtime::RequestUnload()
{
    g_running.store(false, std::memory_order_release);
}

bool Runtime::IsRunning()
{
    return g_running.load(std::memory_order_acquire);
}

HMODULE Runtime::GetModule()
{
    return g_module;
}
