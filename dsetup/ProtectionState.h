#pragma once
#include <windows.h>

struct ProtectionState
{
    volatile LONG filesOk;
    char interfaceUHash[65];
    char interfaceDatHash[65];
};

extern ProtectionState g_ProtectionState;