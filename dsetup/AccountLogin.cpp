#include "AccountLogin.h"

#include <windows.h>

class UNetworkHandler {};

typedef int(__fastcall* RequestAuthLoginFn)(UNetworkHandler*, int, const wchar_t*, const wchar_t*, int);

static constexpr uintptr_t UNETWORK_OFFSET = 0x81F538; // Interlude

static UNetworkHandler** g_UNetwork = NULL;
static RequestAuthLoginFn g_RequestAuthLogin = NULL;

bool AccountLogin_Initialize()
{
    HMODULE engine = GetModuleHandleW(L"engine.dll");
    if (!engine)
        return false;

    g_UNetwork = reinterpret_cast<UNetworkHandler**>(
        reinterpret_cast<uintptr_t>(engine) + UNETWORK_OFFSET
    );

    g_RequestAuthLogin = reinterpret_cast<RequestAuthLoginFn>(
        GetProcAddress(engine, "?RequestAuthLogin@UNetworkHandler@@UAEHPAG0H@Z")
    );

    return g_UNetwork != NULL && g_RequestAuthLogin != NULL;
}

void AccountLogin_Shutdown()
{
    g_UNetwork = NULL;
    g_RequestAuthLogin = NULL;
}

bool AccountLogin_Request(const std::wstring& login, const std::wstring& password)
{
    if (login.empty() || password.empty())
        return false;

    if (!g_UNetwork || !*g_UNetwork || !g_RequestAuthLogin)
        return false;

    g_RequestAuthLogin(*g_UNetwork, 0, login.c_str(), password.c_str(), 0);
    return true;
}
