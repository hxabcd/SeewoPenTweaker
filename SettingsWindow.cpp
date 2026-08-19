#include "SettingsWindow.h"

#include "Config.h"

#include <shellapi.h>

#include <utility>

namespace
{
    constexpr int WindowWidth = 420;
    constexpr int WindowHeight = 206;
    constexpr int Margin = 20;
    constexpr int LabelWidth = 230;
    constexpr int EditWidth = 150;
    constexpr int RowHeight = 32;
}

SettingsWindow::~SettingsWindow()
{
    close();
    if (font_ != nullptr)
    {
        DeleteObject(font_);
    }
}

bool SettingsWindow::registerWindowClass()
{
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = windowProc;
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

    return RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool SettingsWindow::show(
    HWND owner,
    Config &config,
    bool startupEnabled,
    std::function<bool(bool, DWORD &)> startupSetter,
    std::function<void()> savedCallback)
{
    config_ = &config;
    startupEnabled_ = startupEnabled;
    startupSetter_ = std::move(startupSetter);
    savedCallback_ = std::move(savedCallback);

    if (window_ != nullptr)
    {
        loadValues();
        ShowWindow(window_, SW_SHOWNORMAL);
        SetForegroundWindow(window_);
        SetFocus(pressDelayEdit_);
        return true;
    }

    if (!registerWindowClass())
    {
        config_ = nullptr;
        startupSetter_ = {};
        savedCallback_ = {};
        return false;
    }

    RECT windowRect{0, 0, WindowWidth, WindowHeight};
    AdjustWindowRectEx(&windowRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);
    HMONITOR monitor = MonitorFromPoint(cursorPosition, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo))
    {
        x = monitorInfo.rcWork.left +
            (monitorInfo.rcWork.right - monitorInfo.rcWork.left - width) / 2;
        y = monitorInfo.rcWork.top +
            (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - height) / 2;
    }

    const HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        WindowClassName,
        L"SeewoPenTweaker 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        instance_,
        this);
    if (window == nullptr)
    {
        config_ = nullptr;
        startupSetter_ = {};
        savedCallback_ = {};
        return false;
    }

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
    SetForegroundWindow(window);
    SetFocus(pressDelayEdit_);
    return true;
}

void SettingsWindow::close()
{
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
}

LRESULT CALLBACK SettingsWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow *settings = reinterpret_cast<SettingsWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        settings = static_cast<SettingsWindow *>(create->lpCreateParams);
        settings->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(settings));
    }

    return settings == nullptr
               ? DefWindowProcW(window, message, wParam, lParam)
               : settings->handleMessage(message, wParam, lParam);
}

void SettingsWindow::createControls()
{
    HDC deviceContext = GetDC(window_);
    const int dpi = deviceContext == nullptr
                        ? USER_DEFAULT_SCREEN_DPI
                        : GetDeviceCaps(deviceContext, LOGPIXELSY);
    if (deviceContext != nullptr)
    {
        ReleaseDC(window_, deviceContext);
    }

    font_ = CreateFontW(
        -MulDiv(10, dpi, 72),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Microsoft YaHei UI");
    const HFONT font = font_ == nullptr
                           ? static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))
                           : font_;

    const auto createControl = [&](DWORD extendedStyle,
                                   const wchar_t *className,
                                   const wchar_t *text,
                                   DWORD style,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   int controlId) -> HWND
    {
        HWND control = CreateWindowExW(
            extendedStyle,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            x,
            y,
            width,
            height,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
            instance_,
            nullptr);
        if (control != nullptr)
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return control;
    };

    createControl(0, L"STATIC", L"按住左键延时 (毫秒):", SS_LEFT, Margin, Margin, LabelWidth, 24, 0);
    pressDelayEdit_ = createControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
        Margin + LabelWidth,
        Margin - 2,
        EditWidth,
        24,
        PressDelayEdit);

    createControl(
        0,
        L"STATIC",
        L"触发右键时间窗口 (毫秒):",
        SS_LEFT,
        Margin,
        Margin + RowHeight,
        LabelWidth,
        24,
        0);
    pairWindowEdit_ = createControl(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
        Margin + LabelWidth,
        Margin + RowHeight - 2,
        EditWidth,
        24,
        PairWindowEdit);

    startupCheck_ = createControl(
        0,
        L"BUTTON",
        L"开机启动",
        BS_AUTOCHECKBOX | WS_TABSTOP,
        Margin,
        Margin + RowHeight * 2 + 4,
        LabelWidth + EditWidth,
        24,
        StartupCheck);

    autoUpdateCheck_ = createControl(
        0,
        L"BUTTON",
        L"自动检查更新",
        BS_AUTOCHECKBOX | WS_TABSTOP,
        Margin,
        Margin + RowHeight * 3 + 4,
        LabelWidth + EditWidth,
        24,
        AutoUpdateCheck);

    createControl(
        0,
        L"BUTTON",
        L"保存",
        BS_DEFPUSHBUTTON | WS_TABSTOP,
        Margin,
        Margin + RowHeight * 4 + 10,
        100,
        28,
        SaveButton);
    createControl(
        0,
        L"BUTTON",
        L"取消",
        BS_PUSHBUTTON | WS_TABSTOP,
        Margin + 110,
        Margin + RowHeight * 4 + 10,
        100,
        28,
        CancelButton);
    createControl(
        0,
        L"BUTTON",
        L"打开配置目录",
        BS_PUSHBUTTON | WS_TABSTOP,
        Margin + 220,
        Margin + RowHeight * 4 + 10,
        140,
        28,
        OpenConfigButton);
}

void SettingsWindow::loadValues()
{
    if (config_ == nullptr || window_ == nullptr)
    {
        return;
    }

    SetDlgItemInt(window_, PressDelayEdit, config_->pressDelayMs(), FALSE);
    SetDlgItemInt(window_, PairWindowEdit, config_->pairWindowMs(), FALSE);
    SendMessageW(
        startupCheck_,
        BM_SETCHECK,
        startupEnabled_ ? BST_CHECKED : BST_UNCHECKED,
        0);
    SendMessageW(
        autoUpdateCheck_,
        BM_SETCHECK,
        config_->autoUpdateEnabled() ? BST_CHECKED : BST_UNCHECKED,
        0);
}

void SettingsWindow::saveValues()
{
    if (config_ == nullptr)
    {
        return;
    }

    BOOL translated = FALSE;
    const UINT pressDelay = GetDlgItemInt(window_, PressDelayEdit, &translated, FALSE);
    if (!translated || pressDelay < Config::MinimumDelayMs || pressDelay > Config::MaximumDelayMs)
    {
        MessageBoxW(window_, L"按住左键延时必须在 1 到 60000 毫秒之间。", L"设置", MB_OK | MB_ICONWARNING);
        SetFocus(pressDelayEdit_);
        return;
    }

    translated = FALSE;
    const UINT pairWindow = GetDlgItemInt(window_, PairWindowEdit, &translated, FALSE);
    if (!translated || pairWindow < Config::MinimumDelayMs || pairWindow > Config::MaximumDelayMs)
    {
        MessageBoxW(window_, L"触发右键时间窗口必须在 1 到 60000 毫秒之间。", L"设置", MB_OK | MB_ICONWARNING);
        SetFocus(pairWindowEdit_);
        return;
    }

    config_->setPressDelayMs(pressDelay);
    config_->setPairWindowMs(pairWindow);
    config_->setAutoUpdateEnabled(
        SendMessageW(autoUpdateCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    const bool startupEnabled =
        SendMessageW(startupCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;

    DWORD error = ERROR_SUCCESS;
    if (!config_->save(error))
    {
        wchar_t message[256]{};
        wsprintfW(message, L"保存配置文件失败\n错误代码: %lu", error);
        MessageBoxW(window_, message, L"SeewoPenTweaker", MB_OK | MB_ICONERROR);
        return;
    }

    if (startupSetter_ && !startupSetter_(startupEnabled, error))
    {
        wchar_t message[256]{};
        wsprintfW(message, L"设置开机启动失败\n错误代码: %lu", error);
        MessageBoxW(window_, message, L"SeewoPenTweaker", MB_OK | MB_ICONERROR);
        return;
    }

    if (savedCallback_)
    {
        savedCallback_();
    }
    DestroyWindow(window_);
}

void SettingsWindow::openConfigDirectory()
{
    if (config_ == nullptr)
    {
        return;
    }

    std::wstring directory = config_->filePath();
    const size_t separator = directory.find_last_of(L"\\/");
    if (separator != std::wstring::npos)
    {
        directory.resize(separator);
    }

    const HINSTANCE result = ShellExecuteW(
        window_,
        L"open",
        directory.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxW(window_, L"无法打开配置目录。", L"SeewoPenTweaker", MB_OK | MB_ICONERROR);
    }
}

LRESULT SettingsWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        createControls();
        loadValues();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case SaveButton:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                saveValues();
            }
            return 0;
        case CancelButton:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                DestroyWindow(window_);
            }
            return 0;
        case OpenConfigButton:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                openConfigDirectory();
            }
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;

    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));

    case WM_CTLCOLORBTN:
        if (reinterpret_cast<HWND>(lParam) == startupCheck_ ||
            reinterpret_cast<HWND>(lParam) == autoUpdateCheck_)
        {
            SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        break;

    case WM_NCDESTROY:
        window_ = nullptr;
        pressDelayEdit_ = nullptr;
        pairWindowEdit_ = nullptr;
        startupCheck_ = nullptr;
        autoUpdateCheck_ = nullptr;
        if (font_ != nullptr)
        {
            DeleteObject(font_);
            font_ = nullptr;
        }
        startupSetter_ = {};
        config_ = nullptr;
        savedCallback_ = {};
        return 0;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}
