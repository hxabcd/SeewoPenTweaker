#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#define HOTKEY_P 1
#define HOTKEY_Q 2
#define TIMER_P 1
#define EXIT_COMMAND 1001
#define WM_TRAY (WM_APP + 1)

static HWND window_handle;
static NOTIFYICONDATAW tray_icon;
static ULONGLONG last_p_time;
static BOOL left_pressed;

static void send_mouse(DWORD flags)
{
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(input));
}

static void release_left(void)
{
    if (left_pressed) {
        send_mouse(MOUSEEVENTF_LEFTUP);
        left_pressed = FALSE;
    }
}

static void click_right(void)
{
    send_mouse(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP);
}

static void show_menu(void)
{
    HMENU menu = CreatePopupMenu();
    POINT point;

    if (menu == NULL) {
        return;
    }

    AppendMenuW(menu, MF_GRAYED, 0, L"状态: 运行中");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, EXIT_COMMAND, L"退出");

    GetCursorPos(&point);
    SetForegroundWindow(window_handle);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window_handle, NULL);
    PostMessageW(window_handle, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static BOOL add_tray_icon(HWND hwnd)
{
    ZeroMemory(&tray_icon, sizeof(tray_icon));
    tray_icon.cbSize = sizeof(tray_icon);
    tray_icon.hWnd = hwnd;
    tray_icon.uID = 1;
    tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray_icon.uCallbackMessage = WM_TRAY;
    tray_icon.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy_s(tray_icon.szTip, ARRAYSIZE(tray_icon.szTip), L"SeewoPenTweaker");
    return Shell_NotifyIconW(NIM_ADD, &tray_icon);
}

static void remove_tray_icon(void)
{
    Shell_NotifyIconW(NIM_DELETE, &tray_icon);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE:
        if (!RegisterHotKey(hwnd, HOTKEY_P, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'P') ||
            !RegisterHotKey(hwnd, HOTKEY_Q, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'Q') ||
            !add_tray_icon(hwnd)) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_HOTKEY:
        if (w_param == HOTKEY_P) {
            last_p_time = GetTickCount64();
            KillTimer(hwnd, TIMER_P);
            SetTimer(hwnd, TIMER_P, 300, NULL);
        } else if (w_param == HOTKEY_Q) {
            BOOL is_pair;
            KillTimer(hwnd, TIMER_P);
            is_pair = last_p_time != 0 && GetTickCount64() - last_p_time <= 200;
            release_left();
            if (is_pair) {
                click_right();
            }
            last_p_time = 0;
        }
        return 0;

    case WM_TIMER:
        if (w_param == TIMER_P) {
            KillTimer(hwnd, TIMER_P);
            if (!left_pressed) {
                send_mouse(MOUSEEVENTF_LEFTDOWN);
                left_pressed = TRUE;
            }
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(w_param) == EXIT_COMMAND) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_TRAY:
        if (l_param == WM_RBUTTONUP || l_param == WM_LBUTTONUP) {
            show_menu();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_P);
        release_left();
        UnregisterHotKey(hwnd, HOTKEY_P);
        UnregisterHotKey(hwnd, HOTKEY_Q);
        remove_tray_icon();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    const wchar_t class_name[] = L"SeewoPenTweakerWindow";
    WNDCLASSW window_class = {0};
    MSG message;
    HWND hwnd;

    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    window_class.hInstance = instance;
    window_class.lpfnWndProc = window_proc;
    window_class.lpszClassName = class_name;
    window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);

    if (!RegisterClassW(&window_class)) {
        return 1;
    }

    hwnd = CreateWindowExW(
        0,
        class_name,
        L"SeewoPenTweaker",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL) {
        return 1;
    }

    window_handle = hwnd;

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}
