#include "AboutWindow.h"

#include <shellapi.h>

#include <string>
#include <utility>

namespace
{
    constexpr int WindowWidth = 380;
    constexpr int WindowHeight = 150;
    constexpr int Margin = 20;
    constexpr int RowHeight = 28;
    constexpr wchar_t RepositoryUrl[] = L"https://github.com/hxabcd/SeewoPenTweaker";
}

AboutWindow::~AboutWindow()
{
    close();
    if (font_ != nullptr)
    {
        DeleteObject(font_);
    }
}

bool AboutWindow::registerWindowClass()
{
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = windowProc;
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

    return RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool AboutWindow::show(HWND owner, const std::wstring &version)
{
    version_ = version;

    if (window_ != nullptr)
    {
        loadValues();
        ShowWindow(window_, SW_SHOWNORMAL);
        SetForegroundWindow(window_);
        return true;
    }

    if (!registerWindowClass())
    {
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
        L"关于 SeewoPenTweaker",
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
        return false;
    }

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
    SetForegroundWindow(window);
    return true;
}

void AboutWindow::close()
{
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
}

LRESULT CALLBACK AboutWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    AboutWindow *about = reinterpret_cast<AboutWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        about = static_cast<AboutWindow *>(create->lpCreateParams);
        about->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(about));
    }

    return about == nullptr
               ? DefWindowProcW(window, message, wParam, lParam)
               : about->handleMessage(message, wParam, lParam);
}

void AboutWindow::createControls()
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

    const auto createControl = [&](const wchar_t *className,
                                   const wchar_t *text,
                                   DWORD style,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   int controlId) -> HWND
    {
        HWND control = CreateWindowExW(
            0,
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

    createControl(
        L"STATIC",
        L"SeewoPenTweaker",
        SS_LEFT,
        Margin,
        Margin,
        WindowWidth - Margin * 2,
        26,
        0);
    versionLabel_ = createControl(
        L"STATIC",
        L"",
        SS_LEFT,
        Margin,
        Margin + RowHeight,
        WindowWidth - Margin * 2,
        24,
        0);
    createControl(
        L"STATIC",
        L"作者: HxAbCd",
        SS_LEFT,
        Margin,
        Margin + RowHeight * 2,
        WindowWidth - Margin * 2,
        24,
        0);
    createControl(
        L"BUTTON",
        L"打开仓库",
        BS_PUSHBUTTON | WS_TABSTOP,
        Margin,
        Margin + RowHeight * 3,
        140,
        28,
        OpenRepositoryButton);
    createControl(
        L"BUTTON",
        L"关闭",
        BS_DEFPUSHBUTTON | WS_TABSTOP,
        WindowWidth - Margin - 90,
        Margin + RowHeight * 3,
        90,
        28,
        CloseButton);
}

void AboutWindow::loadValues()
{
    if (versionLabel_ != nullptr)
    {
        const std::wstring versionText = L"版本: " + version_;
        SetWindowTextW(versionLabel_, versionText.c_str());
    }
}

void AboutWindow::openRepository()
{
    const HINSTANCE result = ShellExecuteW(
        window_,
        L"open",
        RepositoryUrl,
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxW(window_, L"无法打开仓库页面。", L"SeewoPenTweaker", MB_OK | MB_ICONERROR);
    }
}

LRESULT AboutWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        createControls();
        loadValues();
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == OpenRepositoryButton)
        {
            openRepository();
            return 0;
        }
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == CloseButton)
        {
            DestroyWindow(window_);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;

    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));

    case WM_NCDESTROY:
        window_ = nullptr;
        versionLabel_ = nullptr;
        if (font_ != nullptr)
        {
            DeleteObject(font_);
            font_ = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}
