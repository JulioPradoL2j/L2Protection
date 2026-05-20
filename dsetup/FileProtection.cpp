#include "FileProtection.h"
#include <windows.h>
#include <wincrypt.h>
#include <fstream>

#pragma comment(lib, "Advapi32.lib")

static const wchar_t* FILE_INTERFACE_U = L"interface.u";
static const wchar_t* FILE_INTERFACE_DAT = L"interface.xdat";

//  HASHES ORIGINAIS (VOCÊ VAI GERAR)
static const char* HASH_INTERFACE_U = "f402642f69d433283e3f3e17c9474ade689885e219e78835e6bdbe7d698dd02d";
static const char* HASH_INTERFACE_DAT = "37159dcfb069e7cc913f3f9ddec234437febde37a08c363cce0646c767ef6948";

bool GetFileHash(const wchar_t* file, std::string& outHash)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return false;

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        return false;

    std::ifstream f(file, std::ios::binary);
    if (!f.is_open())
        return false;

    char buffer[4096];

    while (f.read(buffer, sizeof(buffer)) || f.gcount())
    {
        DWORD bytesRead = (DWORD)f.gcount();
        CryptHashData(hHash, (BYTE*)buffer, bytesRead, 0);
    }

    BYTE hash[32];
    DWORD len = 32;

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &len, 0))
        return false;

    char hex[65] = { 0 };
    for (int i = 0; i < 32; i++)
        sprintf_s(hex + i * 2, 3, "%02X", hash[i]);

    outHash = hex;

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return true;
}

FileCheckResult VerifyProtectedFiles()
{
    FileCheckResult result = {};
    result.allOk = true;

    std::string hash;

    // =====================
    // interface.u
    // =====================
    if (!GetFileHash(FILE_INTERFACE_U, hash))
    {
        result.allOk = false;
        result.fileMissing = true;
        result.fileName = FILE_INTERFACE_U;
        return result;
    }

    if (_stricmp(hash.c_str(), HASH_INTERFACE_U) != 0)
    {
        result.allOk = false;
        result.fileChanged = true;
        result.fileName = FILE_INTERFACE_U;
        result.hash = hash;
        return result;
    }

    // =====================
    // interface.dat
    // =====================
    if (!GetFileHash(FILE_INTERFACE_DAT, hash))
    {
        result.allOk = false;
        result.fileMissing = true;
        result.fileName = FILE_INTERFACE_DAT;
        return result;
    }

    if (_stricmp(hash.c_str(), HASH_INTERFACE_DAT) != 0)
    {
        result.allOk = false;
        result.fileChanged = true;
        result.fileName = FILE_INTERFACE_DAT;
        result.hash = hash;
        return result;
    }

    return result;
}