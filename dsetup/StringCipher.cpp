#include "StringCipher.h"

// ============================================================
// XOR ANSI
// ============================================================
std::string StringCipher::DecodeA(const unsigned char* data, size_t len, unsigned char key)
{
    std::string out;
    out.reserve(len);

    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] == 0)
            break;

        out.push_back(data[i] ^ key);
    }

    return out;
}

// ============================================================
// XOR WIDE
// ============================================================
std::wstring StringCipher::DecodeW(const unsigned short* data, size_t len, unsigned short key)
{
    std::wstring out;
    out.reserve(len);

    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] == 0)
            break;

        out.push_back(data[i] ^ key);
    }

    return out;
}

// ============================================================
// ZERO MEMORY
// ============================================================
void StringCipher::ZeroMemorySafe(void* ptr, size_t size)
{
    if (ptr && size > 0)
        SecureZeroMemory(ptr, size);
}