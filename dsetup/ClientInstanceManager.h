#pragma once

#include <windows.h>

class ClientInstanceManager
{
public:
    static bool Acquire();
    static void Release();

    static bool IsOwner();
    static LONG GetCurrentCount();
    static LONG GetMaxClients();

private:
    static bool InitializeSharedMemory();
    static void Cleanup();

private:
    static HANDLE _mutex;
    static HANDLE _mapFile;
    static volatile LONG* _counter;

    static bool _hasSlot;
    static bool _isOwner;

private:
    static constexpr LONG MAX_CLIENTS = 2; // ALTERA AQUI
};