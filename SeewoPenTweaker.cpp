#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <shlobj.h>

#include "AppInfo.h"
#include "AboutWindow.h"
#include "Config.h"
#include "SettingsWindow.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class TrayApp final
{
public:
    explicit TrayApp(HINSTANCE instance)
        : instance_(instance), settingsWindow_(instance), aboutWindow_(instance)
    {
    }

    TrayApp(const TrayApp &) = delete;
    TrayApp &operator=(const TrayApp &) = delete;

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
    static constexpr int UpdateCommand = 1003;
    static constexpr int SettingsCommand = 1004;
    static constexpr int AboutCommand = 1005;
    static constexpr int IconResourceId = 101;
    static constexpr UINT TrayMessage = WM_APP + 1;
    static constexpr UINT UpdateMessage = WM_APP + 2;
    static constexpr wchar_t WindowClassName[] = L"SeewoPenTweakerWindow";
    static constexpr wchar_t MutexName[] = L"Local\\SeewoPenTweaker.SingleInstance";
    static constexpr wchar_t RunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr wchar_t RunValueName[] = L"SeewoPenTweaker";

    struct UpdateResult
    {
        bool success{};
        bool updateAvailable{};
        bool manual{};
        std::wstring latestVersion;
    };

    HINSTANCE instance_{};
    Config config_{};
    SettingsWindow settingsWindow_;
    AboutWindow aboutWindow_;
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
    bool updateNotificationPending_{};
    std::atomic<bool> shuttingDown_{};
    std::atomic<bool> updateCheckRunning_{};
    std::thread updateThread_;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        TrayApp *app = reinterpret_cast<TrayApp *>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE)
        {
            const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
            app = static_cast<TrayApp *>(create->lpCreateParams);
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

    static void setProcessDpiAwareness()
    {
        using SetProcessDpiAwarenessContextFunction = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
        using SetProcessDpiAwareFunction = BOOL(WINAPI *)();

        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32 == nullptr)
        {
            return;
        }

        auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext != nullptr)
        {
            setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }

        auto setSystemAware = reinterpret_cast<SetProcessDpiAwareFunction>(
            GetProcAddress(user32, "SetProcessDPIAware"));
        if (setSystemAware != nullptr)
        {
            setSystemAware();
        }
    }

    static DPI_AWARENESS_CONTEXT setThreadDpiAwareness(DPI_AWARENESS_CONTEXT context)
    {
        using SetThreadDpiAwarenessContextFunction = DPI_AWARENESS_CONTEXT(WINAPI *)(DPI_AWARENESS_CONTEXT);

        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32 == nullptr)
        {
            return nullptr;
        }

        auto setContext = reinterpret_cast<SetThreadDpiAwarenessContextFunction>(
            GetProcAddress(user32, "SetThreadDpiAwarenessContext"));
        return setContext == nullptr ? nullptr : setContext(context);
    }

    static std::array<int, 3> parseVersion(const std::string &version)
    {
        std::array<int, 3> parts{};
        size_t part = 0;
        int value = 0;
        bool hasDigits = false;

        for (const char character : version)
        {
            if (std::isdigit(static_cast<unsigned char>(character)) != 0)
            {
                value = value * 10 + (character - '0');
                hasDigits = true;
            }
            else if (hasDigits)
            {
                if (part < parts.size())
                {
                    parts[part++] = value;
                }
                value = 0;
                hasDigits = false;
            }
        }

        if (hasDigits && part < parts.size())
        {
            parts[part] = value;
        }

        return parts;
    }

    static bool isNewerVersion(const std::string &latestVersion)
    {
        return parseVersion(latestVersion) > parseVersion(AppInfo::Version);
    }

    static bool fetchLatestVersion(std::string &latestVersion)
    {
        HINTERNET session = WinHttpOpen(
            AppInfo::Name,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (session == nullptr)
        {
            return false;
        }

        HINTERNET connection = nullptr;
        HINTERNET request = nullptr;
        const auto closeHandles = [&]()
        {
            if (request != nullptr)
            {
                WinHttpCloseHandle(request);
            }
            if (connection != nullptr)
            {
                WinHttpCloseHandle(connection);
            }
            WinHttpCloseHandle(session);
        };

        WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
        connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connection == nullptr)
        {
            closeHandles();
            return false;
        }

        request = WinHttpOpenRequest(
            connection,
            L"GET",
            AppInfo::GithubApiPath,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (request == nullptr)
        {
            closeHandles();
            return false;
        }

        const wchar_t headers[] =
            L"Accept: application/vnd.github+json\r\n"
            L"User-Agent: SeewoPenTweaker\r\n";
        if (!WinHttpSendRequest(
                request,
                headers,
                static_cast<DWORD>(-1),
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0) ||
            !WinHttpReceiveResponse(request, nullptr))
        {
            closeHandles();
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX) ||
            statusCode != 200)
        {
            closeHandles();
            return false;
        }

        std::string response;
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
        {
            if (available > 1024 * 1024)
            {
                closeHandles();
                return false;
            }

            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read))
            {
                closeHandles();
                return false;
            }
            response.append(buffer.data(), read);
        }

        closeHandles();
        const std::string key = "\"tag_name\"";
        const size_t keyPosition = response.find(key);
        if (keyPosition == std::string::npos)
        {
            return false;
        }

        const size_t valueStart = response.find('"', keyPosition + key.size());
        if (valueStart == std::string::npos)
        {
            return false;
        }

        const size_t valueEnd = response.find('"', valueStart + 1);
        if (valueEnd == std::string::npos || valueEnd <= valueStart + 1)
        {
            return false;
        }

        latestVersion = response.substr(valueStart + 1, valueEnd - valueStart - 1);
        return !latestVersion.empty();
    }

    void checkForUpdates(bool manual)
    {
        if (updateCheckRunning_.exchange(true))
        {
            if (manual)
            {
                showMessage(L"正在检查更新，请稍候。", L"SeewoPenTweaker");
            }
            return;
        }

        if (updateThread_.joinable())
        {
            updateThread_.join();
        }

        updateThread_ = std::thread([this, manual]()
                                    {
            std::string latestVersion;
            const bool success = fetchLatestVersion(latestVersion);
            auto result = std::make_unique<UpdateResult>();
            result->success = success;
            result->updateAvailable = success && isNewerVersion(latestVersion);
            result->manual = manual;
            result->latestVersion.assign(latestVersion.begin(), latestVersion.end());
            updateCheckRunning_ = false;

            if (shuttingDown_ || !PostMessageW(
                    window_,
                    UpdateMessage,
                    0,
                    reinterpret_cast<LPARAM>(result.get())))
            {
                return;
            }

            result.release(); });
    }

    void handleUpdateResult(LPARAM lParam)
    {
        std::unique_ptr<UpdateResult> result(reinterpret_cast<UpdateResult *>(lParam));
        if (!result->success)
        {
            if (result->manual)
            {
                showMessage(L"检查更新失败，请检查网络连接。", L"SeewoPenTweaker");
            }
            return;
        }

        if (!result->updateAvailable)
        {
            if (result->manual)
            {
                showMessage(L"当前已经是最新版本。", L"SeewoPenTweaker");
            }
            return;
        }

        showUpdateNotification(result->latestVersion);
    }

    void showUpdateNotification(const std::wstring &latestVersion)
    {
        updateNotificationPending_ = true;
        trayIcon_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
        lstrcpynW(trayIcon_.szInfoTitle, L"SeewoPenTweaker", ARRAYSIZE(trayIcon_.szInfoTitle));
        const std::wstring message = L"发现新版本 " + latestVersion + L"，点击打开下载页面。";
        lstrcpynW(trayIcon_.szInfo, message.c_str(), ARRAYSIZE(trayIcon_.szInfo));
        trayIcon_.dwInfoFlags = NIIF_INFO;
        trayIcon_.uTimeout = 10000;
        Shell_NotifyIconW(NIM_MODIFY, &trayIcon_);
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

        DWORD configError = ERROR_SUCCESS;
        if (!config_.load(configError))
        {
            showError(L"加载配置文件失败", configError);
            return false;
        }

        setProcessDpiAwareness();

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

        if (config_.autoUpdateEnabled())
        {
            checkForUpdates(false);
        }

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
        using LoadIconMetricFunction = HRESULT(WINAPI *)(HINSTANCE, PCWSTR, int, HICON *);

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
                SetTimer(window_, TimerP, config_.pressDelayMs(), nullptr);
            }
            else if (wParam == HotKeyQ)
            {
                KillTimer(window_, TimerP);
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - lastP_)
                                         .count();
                const bool isPair = hasLastP_ && elapsed <= config_.pairWindowMs();
                releaseLeft();
                if (isPair)
                {
                    clickShortPress();
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
            else if (LOWORD(wParam) == UpdateCommand)
            {
                checkForUpdates(true);
            }
            else if (LOWORD(wParam) == SettingsCommand)
            {
                if (!settingsWindow_.show(
                        window_,
                        config_,
                        isStartupEnabled(),
                        [this](bool enabled, DWORD &error)
                        {
                            return setStartupEnabled(enabled, error);
                        },
                        []() {}))
                {
                    showError(L"打开设置窗口失败");
                }
            }
            else if (LOWORD(wParam) == AboutCommand)
            {
                if (!aboutWindow_.show(window_, AppInfo::VersionWide))
                {
                    showError(L"打开关于窗口失败");
                }
            }
            return 0;

        case TrayMessage:
        {
            const UINT trayEvent = LOWORD(static_cast<ULONG_PTR>(lParam));
            if (trayEvent == NIN_BALLOONUSERCLICK && updateNotificationPending_)
            {
                updateNotificationPending_ = false;
                ShellExecuteW(nullptr, L"open", AppInfo::DownloadUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
            else if (trayEvent == WM_RBUTTONUP || trayEvent == WM_LBUTTONUP)
            {
                showMenu();
            }
        }
            return 0;

        case UpdateMessage:
            handleUpdateResult(lParam);
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
            setThreadDpiAwareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr)
        {
            if (previousContext != nullptr)
            {
                setThreadDpiAwareness(previousContext);
            }
            return;
        }

        AppendMenuW(menu, MF_GRAYED, 0, L"状态: 运行中");
        AppendMenuW(menu, MF_STRING, SettingsCommand, L"设置");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, UpdateCommand, L"检查更新");
        AppendMenuW(menu, MF_STRING, AboutCommand, L"关于");
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
            setThreadDpiAwareness(previousContext);
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

    void clickShortPress() const
    {
        const DWORD buttonDown = config_.shortPressLeftClickEnabled()
                                     ? MOUSEEVENTF_LEFTDOWN
                                     : MOUSEEVENTF_RIGHTDOWN;
        const DWORD buttonUp = config_.shortPressLeftClickEnabled()
                                   ? MOUSEEVENTF_LEFTUP
                                   : MOUSEEVENTF_RIGHTUP;
        sendMouse(buttonDown);
        sendMouse(buttonUp);
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

    bool setStartupEnabled(bool enabled, DWORD &error) const
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
                reinterpret_cast<const BYTE *>(command),
                (pathLength + 3) * sizeof(wchar_t));
        }

        RegCloseKey(key);
        error = static_cast<DWORD>(result);
        return result == ERROR_SUCCESS;
    }

    void shutdown()
    {
        shuttingDown_ = true;
        settingsWindow_.close();
        aboutWindow_.close();
        if (updateThread_.joinable())
        {
            updateThread_.join();
        }

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

    static void showMessage(const wchar_t *message, const wchar_t *title)
    {
        MessageBoxW(nullptr, message, title, MB_OK | MB_ICONINFORMATION);
    }

    static void showError(const wchar_t *action, DWORD error = GetLastError())
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
