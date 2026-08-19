#pragma once

#include <windows.h>

#include <string>

class AboutWindow final
{
public:
    explicit AboutWindow(HINSTANCE instance) : instance_(instance) {}

    AboutWindow(const AboutWindow &) = delete;
    AboutWindow &operator=(const AboutWindow &) = delete;

    ~AboutWindow();

    bool show(HWND owner, const std::wstring &version);
    void close();

private:
    static constexpr wchar_t WindowClassName[] = L"SeewoPenTweakerAboutWindow";
    static constexpr int OpenRepositoryButton = 3001;
    static constexpr int CloseButton = 3002;

    HINSTANCE instance_{};
    HFONT font_{};
    HWND window_{};
    HWND versionLabel_{};
    std::wstring version_;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    bool registerWindowClass();
    void createControls();
    void loadValues();
    void openRepository();
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
};
