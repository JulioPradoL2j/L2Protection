#include "AccountVault.h"

#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Shell32.lib")

static constexpr DWORD VAULT_MAGIC = 0x3256414C; // "LAV2"
static constexpr DWORD VAULT_VERSION = 2;
static constexpr DWORD MAX_ACCOUNTS = 50;
static constexpr DWORD MAX_BLOB_SIZE = 1024 * 64;

static std::wstring GetVaultDirectory()
{
    wchar_t path[MAX_PATH] = {};

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
        return L".";

    std::wstring dir = std::wstring(path) + L"\\LineageII";
    CreateDirectoryW(dir.c_str(), NULL);

    return dir;
}

static std::wstring GetVaultPath()
{
    return GetVaultDirectory() + L"\\accounts.dat";
}

static std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 1)
        return {};

    std::string result(size - 1, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        -1,
        &result[0],
        size,
        NULL,
        NULL
    );

    return result;
}

static std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};

    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, NULL, 0);
    if (size <= 1)
        return {};

    std::wstring result(size - 1, L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        -1,
        &result[0],
        size
    );

    return result;
}

static bool ProtectString(const std::wstring& text, std::vector<BYTE>& encrypted)
{
    encrypted.clear();

    std::string utf8 = WideToUtf8(text);

    DATA_BLOB input = {};
    input.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(utf8.data()));
    input.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB output = {};

    if (!CryptProtectData(&input, L"LineageII Account Vault", NULL, NULL, NULL, 0, &output))
        return false;

    encrypted.assign(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);

    return true;
}

static bool UnprotectString(const std::vector<BYTE>& encrypted, std::wstring& text)
{
    text.clear();

    if (encrypted.empty())
        return false;

    DATA_BLOB input = {};
    input.pbData = const_cast<BYTE*>(encrypted.data());
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output = {};

    if (!CryptUnprotectData(&input, NULL, NULL, NULL, NULL, 0, &output))
        return false;

    std::string utf8(reinterpret_cast<char*>(output.pbData), output.cbData);
    text = Utf8ToWide(utf8);

    LocalFree(output.pbData);
    return true;
}

static void WriteDword(std::ofstream& file, DWORD value)
{
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

static bool ReadDword(std::ifstream& file, DWORD& value)
{
    value = 0;
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

static void WriteBlob(std::ofstream& file, const std::vector<BYTE>& data)
{
    const DWORD size = static_cast<DWORD>(data.size());
    WriteDword(file, size);

    if (size > 0)
        file.write(reinterpret_cast<const char*>(data.data()), size);
}

static bool ReadBlob(std::ifstream& file, std::vector<BYTE>& data)
{
    data.clear();

    DWORD size = 0;
    if (!ReadDword(file, size))
        return false;

    if (size > MAX_BLOB_SIZE)
        return false;

    data.resize(size);

    if (size > 0)
        file.read(reinterpret_cast<char*>(data.data()), size);

    return file.good();
}

static void SortAccounts(std::vector<SavedAccount>& accounts)
{
    std::stable_sort(accounts.begin(), accounts.end(), [](const SavedAccount& a, const SavedAccount& b)
    {
        if (a.favorite != b.favorite)
            return a.favorite > b.favorite;

        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
}

bool AccountVault_Initialize()
{
    GetVaultDirectory();
    return true;
}

void AccountVault_Shutdown()
{
}

static bool LoadV1Legacy(std::ifstream& file, DWORD firstCount, std::vector<SavedAccount>& accounts)
{
    const DWORD count = firstCount;
    if (count > MAX_ACCOUNTS)
        return false;

    for (DWORD i = 0; i < count; i++)
    {
        std::vector<BYTE> nameBlob;
        std::vector<BYTE> loginBlob;
        std::vector<BYTE> passwordBlob;

        if (!ReadBlob(file, nameBlob))
            return false;

        if (!ReadBlob(file, loginBlob))
            return false;

        if (!ReadBlob(file, passwordBlob))
            return false;

        SavedAccount account;

        if (!UnprotectString(nameBlob, account.name))
            continue;

        if (!UnprotectString(loginBlob, account.login))
            continue;

        if (!UnprotectString(passwordBlob, account.password))
            continue;

        account.favorite = false;

        if (!account.login.empty() && !account.password.empty())
            accounts.push_back(account);
    }

    SortAccounts(accounts);
    return true;
}

static bool LoadV2(std::ifstream& file, std::vector<SavedAccount>& accounts)
{
    DWORD version = 0;
    DWORD count = 0;

    if (!ReadDword(file, version))
        return false;

    if (!ReadDword(file, count))
        return false;

    if (version != VAULT_VERSION || count > MAX_ACCOUNTS)
        return false;

    for (DWORD i = 0; i < count; i++)
    {
        DWORD flags = 0;
        if (!ReadDword(file, flags))
            return false;

        std::vector<BYTE> nameBlob;
        std::vector<BYTE> loginBlob;
        std::vector<BYTE> passwordBlob;

        if (!ReadBlob(file, nameBlob))
            return false;

        if (!ReadBlob(file, loginBlob))
            return false;

        if (!ReadBlob(file, passwordBlob))
            return false;

        SavedAccount account;

        if (!UnprotectString(nameBlob, account.name))
            continue;

        if (!UnprotectString(loginBlob, account.login))
            continue;

        if (!UnprotectString(passwordBlob, account.password))
            continue;

        account.favorite = (flags & 1) != 0;

        if (!account.login.empty() && !account.password.empty())
            accounts.push_back(account);
    }

    SortAccounts(accounts);
    return true;
}

bool AccountVault_LoadAccounts(std::vector<SavedAccount>& accounts)
{
    accounts.clear();

    std::ifstream file(GetVaultPath(), std::ios::binary);
    if (!file.is_open())
        return true;

    DWORD first = 0;
    if (!ReadDword(file, first))
        return false;

    if (first == VAULT_MAGIC)
        return LoadV2(file, accounts);

    return LoadV1Legacy(file, first, accounts);
}

static bool SaveAllAccounts(std::vector<SavedAccount> accounts)
{
    SortAccounts(accounts);

    std::ofstream file(GetVaultPath(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;

    WriteDword(file, VAULT_MAGIC);
    WriteDword(file, VAULT_VERSION);
    WriteDword(file, static_cast<DWORD>(accounts.size()));

    for (const SavedAccount& account : accounts)
    {
        std::vector<BYTE> nameBlob;
        std::vector<BYTE> loginBlob;
        std::vector<BYTE> passwordBlob;

        if (!ProtectString(account.name, nameBlob))
            return false;

        if (!ProtectString(account.login, loginBlob))
            return false;

        if (!ProtectString(account.password, passwordBlob))
            return false;

        DWORD flags = account.favorite ? 1 : 0;

        WriteDword(file, flags);
        WriteBlob(file, nameBlob);
        WriteBlob(file, loginBlob);
        WriteBlob(file, passwordBlob);
    }

    return true;
}

bool AccountVault_SaveAccount(const SavedAccount& account)
{
    if (account.login.empty() || account.password.empty())
        return false;

    std::vector<SavedAccount> accounts;
    AccountVault_LoadAccounts(accounts);

    SavedAccount fixed = account;
    if (fixed.name.empty())
        fixed.name = fixed.login;

    for (SavedAccount& current : accounts)
    {
        if (_wcsicmp(current.login.c_str(), fixed.login.c_str()) == 0)
        {
            current = fixed;
            return SaveAllAccounts(accounts);
        }
    }

    if (accounts.size() >= MAX_ACCOUNTS)
        return false;

    accounts.push_back(fixed);
    return SaveAllAccounts(accounts);
}

bool AccountVault_DeleteAccount(const std::wstring& login)
{
    if (login.empty())
        return false;

    std::vector<SavedAccount> accounts;
    AccountVault_LoadAccounts(accounts);

    std::vector<SavedAccount> result;

    for (const SavedAccount& account : accounts)
    {
        if (_wcsicmp(account.login.c_str(), login.c_str()) != 0)
            result.push_back(account);
    }

    return SaveAllAccounts(result);
}

bool AccountVault_SetFavorite(const std::wstring& login, bool favorite)
{
    if (login.empty())
        return false;

    std::vector<SavedAccount> accounts;
    AccountVault_LoadAccounts(accounts);

    bool changed = false;

    for (SavedAccount& account : accounts)
    {
        if (_wcsicmp(account.login.c_str(), login.c_str()) == 0)
        {
            account.favorite = favorite;
            changed = true;
            break;
        }
    }

    if (!changed)
        return false;

    return SaveAllAccounts(accounts);
}
