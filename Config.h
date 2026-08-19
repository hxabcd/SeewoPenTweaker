#pragma once

#include <windows.h>

#include <string>

class Config final
{
public:
    static constexpr UINT DefaultPressDelayMs = 200;
    static constexpr UINT DefaultPairWindowMs = 200;
    static constexpr UINT MinimumDelayMs = 1;
    static constexpr UINT MaximumDelayMs = 60000;

    bool load(DWORD &error);
    bool save(DWORD &error) const;

    bool autoUpdateEnabled() const { return autoUpdateEnabled_; }
    void setAutoUpdateEnabled(bool enabled) { autoUpdateEnabled_ = enabled; }

    bool shortPressLeftClickEnabled() const { return shortPressLeftClickEnabled_; }
    void setShortPressLeftClickEnabled(bool enabled) { shortPressLeftClickEnabled_ = enabled; }

    UINT pressDelayMs() const { return pressDelayMs_; }
    void setPressDelayMs(UINT delayMs) { pressDelayMs_ = delayMs; }

    UINT pairWindowMs() const { return pairWindowMs_; }
    void setPairWindowMs(UINT windowMs) { pairWindowMs_ = windowMs; }

    const std::wstring &filePath() const { return filePath_; }

private:
    static constexpr wchar_t SectionName[] = L"Settings";
    static constexpr wchar_t AutoUpdateKey[] = L"AutoCheckUpdates";
    static constexpr wchar_t ShortPressLeftClickKey[] = L"ShortPressLeftClick";
    static constexpr wchar_t PressDelayKey[] = L"PressDelayMs";
    static constexpr wchar_t PairWindowKey[] = L"PairWindowMs";
    static constexpr wchar_t DirectoryName[] = L"SeewoPenTweaker";
    static constexpr wchar_t FileName[] = L"settings.ini";

    static UINT readDelay(const wchar_t *key, UINT defaultValue, const std::wstring &path);
    static UINT sanitizeDelay(UINT value);

    bool autoUpdateEnabled_{};
    bool shortPressLeftClickEnabled_{};
    UINT pressDelayMs_{DefaultPressDelayMs};
    UINT pairWindowMs_{DefaultPairWindowMs};
    std::wstring filePath_;
};
