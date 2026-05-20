#include "ProtectionUI.h"
#include <cmath>

static std::wstring g_Message;
static HWND g_hWnd = NULL;

static int g_LifeTime = 2500;
static BYTE g_Alpha = 0;

static int g_TargetX = 0;
static int g_TargetY = 0;
static int g_CurrentX = 0;

static int g_State = 0;
// 0 = entrando
// 1 = parado
// 2 = saindo

// ============================================================
// EASING (movimento suave estilo GameGuard)
// ============================================================
float EaseOut(float t)
{
    return 1 - powf(1 - t, 3);
}

// ============================================================
// WINDOW PROC
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);
        SetTimer(hwnd, 1, 16, NULL); // ~60 FPS
        return 0;
    }

    case WM_TIMER:
    {
        static float progress = 0.0f;

        if (g_State == 0) // ENTRANDO
        {
            progress += 0.08f;

            if (progress > 1.0f)
            {
                progress = 1.0f;
                g_State = 1;
            }

            float eased = EaseOut(progress);

            int startX = GetSystemMetrics(SM_CXSCREEN);
            g_CurrentX = (int)(startX - (startX - g_TargetX) * eased);

            g_Alpha = (BYTE)(255 * eased);

            SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);
            SetWindowPos(hwnd, NULL, g_CurrentX, g_TargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        else if (g_State == 1) // PARADO
        {
            g_LifeTime -= 16;

            if (g_LifeTime <= 0)
            {
                g_State = 2;
                progress = 0.0f;
            }
        }
        else if (g_State == 2) // SAINDO
        {
            progress += 0.08f;

            float eased = EaseOut(progress);

            g_Alpha = (BYTE)(255 * (1.0f - eased));

            SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);

            if (g_Alpha <= 5)
            {
                DestroyWindow(hwnd);
            }
        }

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // FUNDO DARK PREMIUM
        HBRUSH bg = CreateSolidBrush(RGB(10, 10, 10));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // BORDA DISCRETA (estilo GameGuard)
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);

        // TEXTO VERMELHO FORTE (ANTI-CHEAT STYLE)
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 40, 40)); // mais agressivo

        HFONT hFont = CreateFontW(
            20, 0, 0, 0,
            FW_BOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI"
        );

        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        DrawTextW(
            hdc,
            g_Message.c_str(),
            -1,
            &rc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================
// THREAD
// ============================================================
DWORD WINAPI UIThread(LPVOID)
{
    const wchar_t CLASS_NAME[] = L"L2_PROTECTION_WINDOW";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    int width = 260;
    int height = 60;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int margin = 20;

    g_TargetX = screenW - width - margin;
    g_TargetY = screenH - height - margin;

    g_CurrentX = screenW;

    g_Alpha = 0;
    g_State = 0;

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME,
        L"",
        WS_POPUP,
        g_CurrentX,
        g_TargetY,
        width,
        height,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );

    if (!g_hWnd)
        return 0;

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// ============================================================
// API
// ============================================================
void ShowProtectionMessage(const std::wstring& text)
{
    g_Message = text;
    CreateThread(NULL, 0, UIThread, NULL, 0, NULL);
}

void ShowProtectionMessageTimed(const std::wstring& text, int seconds)
{
    g_Message = text;
    g_LifeTime = seconds * 1000;

    CreateThread(NULL, 0, UIThread, NULL, 0, NULL);
}