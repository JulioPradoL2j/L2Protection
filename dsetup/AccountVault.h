#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct SavedAccount
{
    std::wstring name;
    std::wstring login;
    std::wstring password;
    bool favorite = false;
};

bool AccountVault_Initialize();
void AccountVault_Shutdown();

bool AccountVault_SaveAccount(const SavedAccount& account);
bool AccountVault_LoadAccounts(std::vector<SavedAccount>& accounts);
bool AccountVault_DeleteAccount(const std::wstring& login);
bool AccountVault_SetFavorite(const std::wstring& login, bool favorite);
