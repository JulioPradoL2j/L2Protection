#pragma once

#include <windows.h>
#include <string>

bool AccountLogin_Initialize();
void AccountLogin_Shutdown();

bool AccountLogin_Request(const std::wstring& login, const std::wstring& password);
