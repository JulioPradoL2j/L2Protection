#pragma once
#include <string>

struct FileCheckResult
{
    bool allOk;
    bool fileChanged;
    bool fileMissing;
    std::wstring fileName;
    std::string hash;
};

bool InitializeFileProtection();
FileCheckResult VerifyProtectedFiles();
bool GetFileHash(const wchar_t* file, std::string& outHash);