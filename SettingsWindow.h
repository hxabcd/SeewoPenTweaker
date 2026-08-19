#pragma once

#include <windows.h>

#include <functional>

class Config;

class SettingsWindow final
{
public:
    explicit SettingsWindow(HINSTANCE instance) : instance_(instance) {}

    SettingsWindow(const SettingsWindow &) = delete;
    SettingsWindow &operator=(const SettingsWindow &) = delete;

    ~SettingsWindow();

    bool show(
        HWND owner,
        Config &config,
        bool startupEnabled,
        std::function<bool(bool, DWORD &)> startupSetter,
        std::function<void()> savedCallback);
    void close();

private:
    static constexpr wchar_t WindowClassName[] = L"SeewoPenTweakerSettingsWindow";
    static constexpr int PressDelayEdit = 2001;
    static constexpr int PairWindowEdit = 2002;
    static constexpr int StartupCheck = 2003;
    static constexpr int AutoUpdateCheck = 2004;
    static constexpr int ShortPressLeftClickCheck = 2005;
    static constexpr int SaveButton = 2006;
    static constexpr int CancelButton = 2007;
    static constexpr int OpenConfigButton = 2008;

    HINSTANCE instance_{};
    HFONT font_{};
    HWND window_{};
    HWND pressDelayEdit_{};
    HWND pairWindowEdit_{};
    HWND startupCheck_{};
    HWND autoUpdateCheck_{};
    HWND shortPressLeftClickCheck_{};
    bool startupEnabled_{};
    Config *config_{};
    std::function<bool(bool, DWORD &)> startupSetter_;
    std::function<void()> savedCallback_;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    bool registerWindowClass();
    void createControls();
    void loadValues();
    void saveValues();
    void openConfigDirectory();
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
};
