#include <windows.h>
#include <shellapi.h>

#include <chrono>

class TrayApp final
{
public:
    explicit TrayApp(HINSTANCE instance) : instance_(instance) {}

    TrayApp(const TrayApp&) = delete;
    TrayApp& operator=(const TrayApp&) = delete;

    ~TrayApp()
    {
        shutdown();
    }

    int run()
    {
        if (!initialize())
        {
            return 1;
        }

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return static_cast<int>(message.wParam);
    }

private:
    static constexpr int HotKeyP = 1;
    static constexpr int HotKeyQ = 2;
    static constexpr int TimerP = 1;
    static constexpr int ExitCommand = 1001;
    static constexpr int StartupCommand = 1002;
    static constexpr int IconResourceId = 101;
    static constexpr UINT TrayMessage = WM_APP + 1;
    static constexpr wchar_t WindowClassName[] = L"SeewoPenTweakerWindow";
    static constexpr wchar_t MutexName[] = L"Local\\SeewoPenTweaker.SingleInstance";
    static constexpr wchar_t RunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr wchar_t RunValueName[] = L"SeewoPenTweaker";

    HINSTANCE instance_{};
    HWND window_{};
    HANDLE instanceMutex_{};
    NOTIFYICONDATAW trayIcon_{};
    HICON icon_{};
    bool ownsIcon_{};
    std::chrono::steady_clock::time_point lastP_{};
    bool hasLastP_{};
    bool leftPressed_{};
    bool trayAdded_{};
    bool hotkeyPRegistered_{};
    bool hotkeyQRegistered_{};

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        TrayApp* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            app = static_cast<TrayApp*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            return TRUE;
        }

        if (message == WM_CREATE)
        {
            return 0;
        }

        return app == nullptr
            ? DefWindowProcW(window, message, wParam, lParam)
            : app->handleMessage(message, wParam, lParam);
    }

    bool initialize()
    {
        instanceMutex_ = CreateMutexW(nullptr, TRUE, MutexName);
        const DWORD mutexError = GetLastError();
        if (instanceMutex_ == nullptr)
        {
            showError(L"创建单实例锁失败", mutexError);
            return false;
        }
        if (mutexError == ERROR_ALREADY_EXISTS)
        {
            showMessage(L"程序已经在运行。", L"SeewoPenTweaker");
            return false;
        }

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSW windowClass{};
        windowClass.hInstance = instance_;
        windowClass.lpfnWndProc = windowProc;
        windowClass.lpszClassName = WindowClassName;
        windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IconResourceId));

        if (RegisterClassW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            showError(L"注册窗口类失败");
            return false;
        }

        window_ = CreateWindowExW(
            0,
            WindowClassName,
            L"SeewoPenTweaker",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            instance_,
            this);

        if (window_ == nullptr)
        {
            showError(L"创建隐藏窗口失败");
            return false;
        }

        if (!addTrayIcon())
        {
            showError(L"创建托盘图标失败");
            return false;
        }

        constexpr UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
        if (!RegisterHotKey(window_, HotKeyP, modifiers, 'P'))
        {
            showError(L"注册 P 快捷键失败，可能已被占用");
            return false;
        }
        hotkeyPRegistered_ = true;

        if (!RegisterHotKey(window_, HotKeyQ, modifiers, 'Q'))
        {
            showError(L"注册 Q 快捷键失败，可能已被占用");
            return false;
        }
        hotkeyQRegistered_ = true;

        return true;
    }

    bool addTrayIcon()
    {
        if (!loadScaledIcon())
        {
            return false;
        }

        trayIcon_.cbSize = sizeof(trayIcon_);
        trayIcon_.hWnd = window_;
        trayIcon_.uID = 1;
        trayIcon_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        trayIcon_.uCallbackMessage = TrayMessage;
        trayIcon_.hIcon = icon_;
        lstrcpynW(trayIcon_.szTip, L"SeewoPenTweaker", ARRAYSIZE(trayIcon_.szTip));

        if (!Shell_NotifyIconW(NIM_ADD, &trayIcon_))
        {
            return false;
        }

        trayIcon_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &trayIcon_);
        trayAdded_ = true;
        return true;
    }

    bool loadScaledIcon()
    {
        using LoadIconMetricFunction = HRESULT(WINAPI*)(HINSTANCE, PCWSTR, int, HICON*);

        HMODULE commonControls = LoadLibraryW(L"comctl32.dll");
        if (commonControls != nullptr)
        {
            auto loadIconMetric = reinterpret_cast<LoadIconMetricFunction>(
                GetProcAddress(commonControls, "LoadIconMetric"));
            if (loadIconMetric != nullptr &&
                SUCCEEDED(loadIconMetric(instance_, MAKEINTRESOURCEW(IconResourceId), 0, &icon_)))
            {
                ownsIcon_ = true;
                FreeLibrary(commonControls);
                return true;
            }
            FreeLibrary(commonControls);
        }

        HICON baseIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IconResourceId));
        if (baseIcon == nullptr)
        {
            return false;
        }

        const int size = GetSystemMetrics(SM_CXSMICON);
        icon_ = static_cast<HICON>(CopyImage(
            baseIcon,
            IMAGE_ICON,
            size,
            size,
            LR_COPYFROMRESOURCE));
        if (icon_ != nullptr)
        {
            ownsIcon_ = true;
            return true;
        }

        icon_ = baseIcon;
        ownsIcon_ = false;
        return true;
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_HOTKEY:
            if (wParam == HotKeyP)
            {
                lastP_ = std::chrono::steady_clock::now();
                hasLastP_ = true;
                KillTimer(window_, TimerP);
                SetTimer(window_, TimerP, 300, nullptr);
            }
            else if (wParam == HotKeyQ)
            {
                KillTimer(window_, TimerP);
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - lastP_).count();
                const bool isPair = hasLastP_ && elapsed <= 200;
                releaseLeft();
                if (isPair)
                {
                    clickRight();
                }
                hasLastP_ = false;
            }
            return 0;

        case WM_TIMER:
            if (wParam == TimerP)
            {
                KillTimer(window_, TimerP);
                if (!leftPressed_)
                {
                    sendMouse(MOUSEEVENTF_LEFTDOWN);
                    leftPressed_ = true;
                }
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ExitCommand)
            {
                DestroyWindow(window_);
            }
            else if (LOWORD(wParam) == StartupCommand)
            {
                const bool enabled = !isStartupEnabled();
                DWORD error = ERROR_SUCCESS;
                if (!setStartupEnabled(enabled, error))
                {
                    showError(L"设置开机启动失败", error);
                }
            }
            return 0;

        case TrayMessage:
            {
                const UINT trayEvent = LOWORD(static_cast<ULONG_PTR>(lParam));
                if (trayEvent == WM_RBUTTONUP || trayEvent == WM_LBUTTONUP)
                {
                    showMenu();
                }
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(window_, message, wParam, lParam);
    }

    void showMenu()
    {
        const DPI_AWARENESS_CONTEXT previousContext =
            SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr)
        {
            if (previousContext != nullptr)
            {
                SetThreadDpiAwarenessContext(previousContext);
            }
            return;
        }

        AppendMenuW(menu, MF_GRAYED, 0, L"状态: 运行中");
        AppendMenuW(
            menu,
            MF_STRING | (isStartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
            StartupCommand,
            L"开机启动");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ExitCommand, L"退出");

        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(window_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window_, nullptr);
        PostMessageW(window_, WM_NULL, 0, 0);
        DestroyMenu(menu);

        if (previousContext != nullptr)
        {
            SetThreadDpiAwarenessContext(previousContext);
        }
    }

    void pressLeft()
    {
        if (!leftPressed_)
        {
            sendMouse(MOUSEEVENTF_LEFTDOWN);
            leftPressed_ = true;
        }
    }

    void releaseLeft()
    {
        if (leftPressed_)
        {
            sendMouse(MOUSEEVENTF_LEFTUP);
            leftPressed_ = false;
        }
    }

    static void clickRight()
    {
        sendMouse(MOUSEEVENTF_RIGHTDOWN);
        sendMouse(MOUSEEVENTF_RIGHTUP);
    }

    static void sendMouse(DWORD flags)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = flags;
        SendInput(1, &input, sizeof(input));
    }

    bool isStartupEnabled() const
    {
        HKEY key{};
        const LONG openResult = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            RunKeyPath,
            0,
            KEY_QUERY_VALUE,
            &key);
        if (openResult != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD type = 0;
        const LONG queryResult = RegQueryValueExW(
            key,
            RunValueName,
            nullptr,
            &type,
            nullptr,
            nullptr);
        RegCloseKey(key);
        return queryResult == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
    }

    bool setStartupEnabled(bool enabled, DWORD& error) const
    {
        HKEY key{};
        LONG result = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            RunKeyPath,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr);
        if (result != ERROR_SUCCESS)
        {
            error = static_cast<DWORD>(result);
            return false;
        }

        if (!enabled)
        {
            result = RegDeleteValueW(key, RunValueName);
            if (result == ERROR_FILE_NOT_FOUND)
            {
                result = ERROR_SUCCESS;
            }
        }
        else
        {
            wchar_t path[32768]{};
            const DWORD pathLength = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
            if (pathLength == 0 || pathLength + 3 >= ARRAYSIZE(path))
            {
                RegCloseKey(key);
                error = GetLastError();
                if (error == ERROR_SUCCESS)
                {
                    error = ERROR_INSUFFICIENT_BUFFER;
                }
                return false;
            }

            wchar_t command[32768]{};
            command[0] = L'"';
            CopyMemory(command + 1, path, pathLength * sizeof(wchar_t));
            command[pathLength + 1] = L'"';
            command[pathLength + 2] = L'\0';
            result = RegSetValueExW(
                key,
                RunValueName,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command),
                (pathLength + 3) * sizeof(wchar_t));
        }

        RegCloseKey(key);
        error = static_cast<DWORD>(result);
        return result == ERROR_SUCCESS;
    }

    void shutdown()
    {
        if (window_ != nullptr)
        {
            KillTimer(window_, TimerP);
            releaseLeft();
            if (hotkeyPRegistered_)
            {
                UnregisterHotKey(window_, HotKeyP);
            }
            if (hotkeyQRegistered_)
            {
                UnregisterHotKey(window_, HotKeyQ);
            }
            if (trayAdded_)
            {
                Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
            }
            if (ownsIcon_ && icon_ != nullptr)
            {
                DestroyIcon(icon_);
            }
            DestroyWindow(window_);
            window_ = nullptr;
        }

        if (instanceMutex_ != nullptr)
        {
            CloseHandle(instanceMutex_);
            instanceMutex_ = nullptr;
        }
    }

    static void showMessage(const wchar_t* message, const wchar_t* title)
    {
        MessageBoxW(nullptr, message, title, MB_OK | MB_ICONINFORMATION);
    }

    static void showError(const wchar_t* action, DWORD error = GetLastError())
    {
        wchar_t message[256]{};
        wsprintfW(message, L"%ls\n错误代码: %lu", action, error);
        MessageBoxW(nullptr, message, L"SeewoPenTweaker", MB_OK | MB_ICONERROR);
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    return TrayApp(instance).run();
}
