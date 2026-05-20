#ifndef HOOK_H
#define HOOK_H

#include <Windows.h>
#include <cstddef>

namespace Hook
{
    struct Trampoline
    {
        unsigned char* target;
        unsigned char* gateway;
        std::size_t stolenBytes;
        unsigned char original[16];

        Trampoline()
            : target(nullptr), gateway(nullptr), stolenBytes(0)
        {
            ZeroMemory(original, sizeof(original));
        }
    };

    bool WriteJump(void* src, const void* dst);
    bool InstallHook(void* target, void* detour, std::size_t stolenBytes, Trampoline& outHook);
    bool RemoveHook(Trampoline& hook);
}

// Compatibilidade com seu código antigo.
// Retorna o gateway/trampoline para chamar a função original.
unsigned int splice(unsigned char* addr, void* hook_fn);

// Versão mais segura: você informa quantos bytes quer roubar.
unsigned int spliceEx(unsigned char* addr, void* hook_fn, std::size_t stolenBytes);

#endif