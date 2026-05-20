#include "ClientInstanceManager.h"

static const wchar_t* MUTEX_NAME = L"Global\\BAN_L2JDEV_CLIENT_MUTEX";
static const wchar_t* MAP_NAME = L"Global\\BAN_L2JDEV_CLIENT_COUNTER";

// ==========================
// STATIC INIT
// ==========================
HANDLE ClientInstanceManager::_mutex = NULL;
HANDLE ClientInstanceManager::_mapFile = NULL;
volatile LONG* ClientInstanceManager::_counter = NULL;

bool ClientInstanceManager::_hasSlot = false;
bool ClientInstanceManager::_isOwner = false;

// ==========================
// SHARED MEMORY INIT
// ==========================
bool ClientInstanceManager::InitializeSharedMemory()
{
    if (_mapFile && _counter)
        return true;

    _mapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(LONG),
        MAP_NAME
    );

    if (!_mapFile)
        return false;

    _counter = (volatile LONG*)MapViewOfFile(
        _mapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(LONG)
    );

    if (!_counter)
    {
        CloseHandle(_mapFile);
        _mapFile = NULL;
        return false;
    }

    // primeira instância
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *_counter = 0;
    }

    return true;
}

// ==========================
// ACQUIRE SLOT
// ==========================
bool ClientInstanceManager::Acquire()
{
    if (_hasSlot)
        return _isOwner;

    _mutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (!_mutex)
        return false;

    DWORD result = WaitForSingleObject(_mutex, 5000);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
    {
        CloseHandle(_mutex);
        _mutex = NULL;
        return false;
    }

    bool success = false;

    do
    {
        if (!InitializeSharedMemory())
            break;

        LONG count = InterlockedCompareExchange((LONG*)_counter, 0, 0);

        if (count >= MAX_CLIENTS)
            break;

        InterlockedIncrement((LONG*)_counter);

        _hasSlot = true;
        _isOwner = true;
        success = true;

    } while (false);

    ReleaseMutex(_mutex);

    if (!success)
    {
        Cleanup();
    }

    return success;
}

// ==========================
// RELEASE SLOT
// ==========================
void ClientInstanceManager::Release()
{
    if (!_hasSlot)
    {
        Cleanup();
        return;
    }

    if (_mutex)
    {
        DWORD result = WaitForSingleObject(_mutex, 3000);

        if ((result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) && _counter)
        {
            LONG count = InterlockedCompareExchange((LONG*)_counter, 0, 0);

            if (count > 0)
                InterlockedDecrement((LONG*)_counter);

            ReleaseMutex(_mutex);
        }
    }

    _hasSlot = false;
    _isOwner = false;

    Cleanup();
}

// ==========================
// CLEANUP
// ==========================
void ClientInstanceManager::Cleanup()
{
    if (_counter)
    {
        UnmapViewOfFile((LPCVOID)_counter);
        _counter = NULL;
    }

    if (_mapFile)
    {
        CloseHandle(_mapFile);
        _mapFile = NULL;
    }

    if (_mutex)
    {
        CloseHandle(_mutex);
        _mutex = NULL;
    }
}

// ==========================
// GETTERS
// ==========================
bool ClientInstanceManager::IsOwner()
{
    return _isOwner;
}

LONG ClientInstanceManager::GetCurrentCount()
{
    if (!_counter)
        return 0;

    return *_counter;
}

LONG ClientInstanceManager::GetMaxClients()
{
    return MAX_CLIENTS;
}