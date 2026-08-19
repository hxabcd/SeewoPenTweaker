#include "Config.h"

#include <shlobj.h>

#include <string>

UINT Config::readDelay(const wchar_t *key, UINT defaultValue, const std::wstring &path)
{
    const int value = GetPrivateProfileIntW(SectionName, key, static_cast<int>(defaultValue), path.c_str());
    if (value < 0)
    {
        return defaultValue;
    }

    return sanitizeDelay(static_cast<UINT>(value));
}

UINT Config::sanitizeDelay(UINT value)
{
    if (value < MinimumDelayMs)
    {
        return MinimumDelayMs;
    }
    if (value > MaximumDelayMs)
    {
        return MaximumDelayMs;
    }
    return value;
}

bool Config::load(DWORD &error)
{
    wchar_t appDataPath[MAX_PATH]{};
    const HRESULT result = SHGetFolderPathW(
        nullptr,
        CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr,
        SHGFP_TYPE_CURRENT,
        appDataPath);
    if (FAILED(result))
    {
        error = static_cast<DWORD>(result);
        return false;
    }

    const std::wstring directory = std::wstring(appDataPath) + L"\\" + DirectoryName;
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        error = GetLastError();
        return false;
    }

    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY;
        return false;
    }

    filePath_ = directory + L"\\" + FileName;
    autoUpdateEnabled_ = GetPrivateProfileIntW(
                             SectionName,
                             AutoUpdateKey,
                             0,
                             filePath_.c_str()) != 0;
    pressDelayMs_ = readDelay(PressDelayKey, DefaultPressDelayMs, filePath_);
    pairWindowMs_ = readDelay(PairWindowKey, DefaultPairWindowMs, filePath_);

    if (GetFileAttributesW(filePath_.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        return save(error);
    }

    error = ERROR_SUCCESS;
    return true;
}

bool Config::save(DWORD &error) const
{
    if (filePath_.empty())
    {
        error = ERROR_PATH_NOT_FOUND;
        return false;
    }

    const std::wstring pressDelay = std::to_wstring(sanitizeDelay(pressDelayMs_));
    const std::wstring pairWindow = std::to_wstring(sanitizeDelay(pairWindowMs_));
    const wchar_t *autoUpdate = autoUpdateEnabled_ ? L"1" : L"0";

    if (!WritePrivateProfileStringW(SectionName, AutoUpdateKey, autoUpdate, filePath_.c_str()) ||
        !WritePrivateProfileStringW(SectionName, PressDelayKey, pressDelay.c_str(), filePath_.c_str()) ||
        !WritePrivateProfileStringW(SectionName, PairWindowKey, pairWindow.c_str(), filePath_.c_str()))
    {
        error = GetLastError();
        if (error == ERROR_SUCCESS)
        {
            error = ERROR_WRITE_FAULT;
        }
        return false;
    }

    error = ERROR_SUCCESS;
    return true;
}
