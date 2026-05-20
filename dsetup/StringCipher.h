#pragma once

#include <windows.h>
#include <string>
#include <vector>

// ============================================================
// SECURE STRING CIPHER
// ============================================================
// - Ofusca strings no binário
// - Decodifica somente em runtime
// - Limpa memória automaticamente
// ============================================================

class SecureString
{
public:
    SecureString() {}
    ~SecureString()
    {
        SecureZeroMemory(_data.data(), _data.size());
    }

    void set(const std::string& str)
    {
        _data.assign(str.begin(), str.end());
    }

    const char* c_str()
    {
        return _data.data();
    }

    std::string str() const
    {
        return std::string(_data.begin(), _data.end());
    }

private:
    std::vector<char> _data;
};

class SecureWString
{
public:
    SecureWString() {}
    ~SecureWString()
    {
        SecureZeroMemory(_data.data(), _data.size() * sizeof(wchar_t));
    }

    void set(const std::wstring& str)
    {
        _data.assign(str.begin(), str.end());
    }

    const wchar_t* c_str()
    {
        return _data.data();
    }

    std::wstring str() const
    {
        return std::wstring(_data.begin(), _data.end());
    }

private:
    std::vector<wchar_t> _data;
};

// ============================================================
// STRING CIPHER CORE
// ============================================================
class StringCipher
{
public:
    // XOR Decode ANSI
    static std::string DecodeA(const unsigned char* data, size_t len, unsigned char key);

    // XOR Decode UNICODE
    static std::wstring DecodeW(const unsigned short* data, size_t len, unsigned short key);

    // Safe zero
    static void ZeroMemorySafe(void* ptr, size_t size);
};

// ============================================================
// MACROS PARA FACILIDADE
// ============================================================

// ANSI
#define DECODE_STR_A(name, data, key) \
    SecureString name; \
    name.set(StringCipher::DecodeA(data, sizeof(data), key));

// WIDE
#define DECODE_STR_W(name, data, key) \
    SecureWString name; \
    name.set(StringCipher::DecodeW(data, sizeof(data) / sizeof(unsigned short), key));