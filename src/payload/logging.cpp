#include "logging.hpp"

#include <cstdarg>
#include <cstdlib>
#include <cwchar>

namespace Logging {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
wchar_t g_path[MAX_PATH] = {};

} // namespace

bool Initialise(HMODULE module) {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, _countof(path));
    if (length == 0 || length >= _countof(path))
        return false;
    wchar_t* slash = std::wcsrchr(path, L'\\');
    if (!slash)
        return false;
    *(slash + 1) = L'\0';
    constexpr wchar_t name[] = L"DeadSpaceCompleteInputFix.log";
    if (std::wcslen(path) + _countof(name) > _countof(path))
        return false;
    wcscat_s(path, _countof(path), name);
    std::wmemcpy(g_path, path, _countof(g_path));
    return true;
}

void Write(const wchar_t* format, ...) {
    if (!format || g_path[0] == L'\0')
        return;

    wchar_t message[1536] = {};
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t line[2048] = {};
    _snwprintf_s(
        line, _countof(line), _TRUNCATE,
        L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] [pid %lu] %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, static_cast<unsigned long>(GetCurrentProcessId()), message);

    AcquireSRWLockExclusive(&g_lock);
    HANDLE file = CreateFileW(
        g_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const int required = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
        if (required > 1) {
            char utf8[4096] = {};
            const int converted = WideCharToMultiByte(
                CP_UTF8, 0, line, -1, utf8, static_cast<int>(sizeof(utf8)), nullptr, nullptr);
            if (converted > 1) {
                DWORD written = 0;
                WriteFile(file, utf8, static_cast<DWORD>(converted - 1), &written, nullptr);
            }
        }
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&g_lock);
    OutputDebugStringW(line);
}

} // namespace Logging
