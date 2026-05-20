#include "L2SplashClass.h"
// funtion Splash
void ShowSplash()
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = "L2SplashClass";

    RegisterClass(&wc);

    int width = 256;
    int height = 125;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int x = screenW - width - 20;
    int y = screenH - height - 60; // acima do relógio

    g_hSplash = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "L2SplashClass",
        "",
        WS_POPUP,
        x, y,
        width, height,
        NULL, NULL,
        g_hInstance,
        NULL
    );

    ShowWindow(g_hSplash, SW_SHOW);
    UpdateWindow(g_hSplash);

    // 🔥 LOAD IMAGE
    HRSRC res = FindResource(g_hInstance, MAKEINTRESOURCE(IDB_SPLASHLOAD), RT_RCDATA);
    if (!res) return;

    DWORD size = SizeofResource(g_hInstance, res);
    HGLOBAL hResData = LoadResource(g_hInstance, res);

    void* pData = LockResource(hResData);

    // salva temporário
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "l2_splash.jpg");

    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    fwrite(pData, 1, size, f);
    fclose(f);

    // mostra via GDI
    HBITMAP hBitmap = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

    if (hBitmap)
    {
        HDC hdc = GetDC(g_hSplash);
        HDC memDC = CreateCompatibleDC(hdc);

        SelectObject(memDC, hBitmap);

        BITMAP bm;
        GetObject(hBitmap, sizeof(bm), &bm);

        BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);

        DeleteDC(memDC);
        ReleaseDC(g_hSplash, hdc);
    }
}