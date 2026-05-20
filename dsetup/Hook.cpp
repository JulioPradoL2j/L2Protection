#include "Hook.h"

#include <cstdint>

namespace
{
    constexpr std::size_t kMinJumpSize = 5;
    constexpr std::size_t kMaxSavedBytes = 16;

    bool ProtectMemory(void* address, std::size_t size, DWORD newProtect, DWORD& oldProtect)
    {
        return VirtualProtect(address, size, newProtect, &oldProtect) != FALSE;
    }

    void Flush(void* address, std::size_t size)
    {
        FlushInstructionCache(GetCurrentProcess(), address, size);
    }
}

namespace Hook
{
    bool WriteJump(void* src, const void* dst)
    {
        if (!src || !dst)
            return false;

        auto* pSrc = static_cast<unsigned char*>(src);
        auto srcAddr = reinterpret_cast<std::uintptr_t>(src);
        auto dstAddr = reinterpret_cast<std::uintptr_t>(dst);

        std::intptr_t rel = static_cast<std::intptr_t>(dstAddr - (srcAddr + kMinJumpSize));

        DWORD oldProtect = 0;
        if (!ProtectMemory(src, kMinJumpSize, PAGE_EXECUTE_READWRITE, oldProtect))
            return false;

        pSrc[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(pSrc + 1) = static_cast<std::int32_t>(rel);

        DWORD temp = 0;
        VirtualProtect(src, kMinJumpSize, oldProtect, &temp);
        Flush(src, kMinJumpSize);
        return true;
    }

    bool InstallHook(void* target, void* detour, std::size_t stolenBytes, Trampoline& outHook)
    {
        if (!target || !detour)
            return false;

        if (stolenBytes < kMinJumpSize || stolenBytes > kMaxSavedBytes)
            return false;

        auto* pTarget = static_cast<unsigned char*>(target);

        auto* gateway = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, stolenBytes + kMinJumpSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

        if (!gateway)
            return false;

        memcpy(outHook.original, pTarget, stolenBytes);
        memcpy(gateway, pTarget, stolenBytes);

        auto gatewayJmpSrc = gateway + stolenBytes;
        auto returnAddr = pTarget + stolenBytes;

        gatewayJmpSrc[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(gatewayJmpSrc + 1) =
            static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(returnAddr) -
                (reinterpret_cast<std::uintptr_t>(gatewayJmpSrc) + kMinJumpSize));

        DWORD oldProtect = 0;
        if (!ProtectMemory(pTarget, stolenBytes, PAGE_EXECUTE_READWRITE, oldProtect))
        {
            VirtualFree(gateway, 0, MEM_RELEASE);
            return false;
        }

        memset(pTarget, 0x90, stolenBytes);
        pTarget[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(pTarget + 1) =
            static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(detour) -
                (reinterpret_cast<std::uintptr_t>(pTarget) + kMinJumpSize));

        DWORD temp = 0;
        VirtualProtect(pTarget, stolenBytes, oldProtect, &temp);
        Flush(pTarget, stolenBytes);

        outHook.target = pTarget;
        outHook.gateway = gateway;
        outHook.stolenBytes = stolenBytes;

        return true;
    }

    bool RemoveHook(Trampoline& hook)
    {
        if (!hook.target || hook.stolenBytes == 0)
            return false;

        DWORD oldProtect = 0;
        if (!ProtectMemory(hook.target, hook.stolenBytes, PAGE_EXECUTE_READWRITE, oldProtect))
            return false;

        memcpy(hook.target, hook.original, hook.stolenBytes);

        DWORD temp = 0;
        VirtualProtect(hook.target, hook.stolenBytes, oldProtect, &temp);
        Flush(hook.target, hook.stolenBytes);

        if (hook.gateway)
            VirtualFree(hook.gateway, 0, MEM_RELEASE);

        hook.target = nullptr;
        hook.gateway = nullptr;
        hook.stolenBytes = 0;
        ZeroMemory(hook.original, sizeof(hook.original));

        return true;
    }
}

unsigned int spliceEx(unsigned char* addr, void* hook_fn, std::size_t stolenBytes)
{
    static Hook::Trampoline hook;

    if (!addr || !hook_fn)
        return 0;

    if (!Hook::InstallHook(addr, hook_fn, stolenBytes, hook))
        return 0;

    return reinterpret_cast<unsigned int>(hook.gateway);
}

unsigned int splice(unsigned char* addr, void* hook_fn)
{
    return spliceEx(addr, hook_fn, 5);
}