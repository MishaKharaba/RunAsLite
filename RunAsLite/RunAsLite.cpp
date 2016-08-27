#include "targetver.h"
#include <stdio.h>
#include <Windows.h>

static wchar_t  domainUser[128];
static wchar_t* pUser;
static wchar_t* pDomain;
static wchar_t  password[128];
static wchar_t  commandLine[8192];
static bool     isNoProfile;
static bool     isNetonly;

static void displayHelp()
{
    printf("\n");
    printf("RunAsLite v1.0 - lite version of runas\n");
    printf("\n");
    printf("Usage:\n  RunAsLite [/noprofile] [/netonly] <[domain\\]user> <password> <commandline>\n");
    printf("\n");
    printf("Example:\n  RunAsLite /netonly somedomain\\user1 my_pwd1 cmd.exe /c dir\n");
}

static void displayWin32ApiError(const char* routineName)
{
    DWORD errorCode = GetLastError();
    wchar_t* msg = nullptr;
    int i = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0,
        reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    if (i == 0)
    {
        fprintf(stderr, "FormatMessage failed for error code %i.\n", errorCode);
        return;
    }
    fwprintf(stderr, L"Error code %i returned from %S.\n%s\n", errorCode, routineName, msg);
    LocalFree(msg);
}

static const wchar_t* skipSpaces(const wchar_t* s)
{
    while (*s == L' ') s++;
    return s;
}

static const wchar_t* skipNonSpaces(const wchar_t* s)
{
    while (*s != 0 && *s != L' ') s++;
    return s;
}

static const wchar_t* skipPastChar(const wchar_t* s, wchar_t c)
{
    while (*s != 0)
    {
        if (*s == c) { s++; break; }
        s++;
    }
    return s;
}

static const wchar_t* getNextArg(const wchar_t* s, wchar_t* wBuf, int wBufSize)
{
    const wchar_t* s1 = skipSpaces(s);
    const wchar_t* s2 = skipNonSpaces(s1);
    wcsncpy_s(wBuf, wBufSize, s1, s2 - s1);
    return s2;
}

static bool parseCommandLineParms()
{
    const wchar_t* s = GetCommandLine();

    wchar_t arg[128];

    s = skipSpaces(s);
    if (*s == L'"')
        s = skipPastChar(s + 1, L'"');
    else
        s = skipNonSpaces(s);

    s = skipSpaces(s);
    if (*s == 0)
    {
        displayHelp();
        return false;
    }

    int wasUserPwd = 0;
    while (*s != 0)
    {
        const wchar_t* prev_s = s;
        s = getNextArg(s, arg, sizeof(arg) / sizeof(wchar_t));
        if (wcscmp(arg, L"/noprofile") == 0)
        {
            isNoProfile = true;
        }
        else if (wcscmp(arg, L"/netonly") == 0)
        {
            isNetonly = true;
            isNoProfile = true;
        }
        else if (wasUserPwd == 0)
        {
            wcsncpy_s(domainUser, arg, sizeof(domainUser) / sizeof(wchar_t));
            wasUserPwd = 1;
        }
        else if (wasUserPwd == 1)
        {
            wcsncpy_s(password, arg, sizeof(password) / sizeof(wchar_t));
            wasUserPwd = 2;
        }
        else
        {
            s = prev_s;
            break;
        }
    }
    s = skipSpaces(s);
    wcsncpy_s(commandLine, s, sizeof(commandLine) / sizeof(wchar_t));
    if (commandLine[0] == 0)
    {
        fprintf(stderr, "Missing command-line arguments.\n");
        return false;
    }
    return true;
}

static void parseDomainUser()
{
    wchar_t* p = wcschr(domainUser, '\\');
    if (p != nullptr)
    {
        *p = 0;
        pDomain = domainUser;
        pUser = p + 1;
    }
    else
    {
        pUser = domainUser;
    }
}

int main()
{
    if (!parseCommandLineParms())
        return 1;
    parseDomainUser();

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    DWORD flags = 0;
    if (!isNoProfile)
        flags |= LOGON_WITH_PROFILE;
    if (isNetonly)
        flags |= LOGON_NETCREDENTIALS_ONLY;

    fwprintf(stderr, L"User     : %s\n", pUser);
    fwprintf(stderr, L"Domain   : %s\n", pDomain);
    fwprintf(stderr, L"Starting : %s\n", commandLine);

    BOOL ok = CreateProcessWithLogonW(pUser, pDomain, password,
        flags, nullptr, commandLine, 0, nullptr, nullptr,
        &si, &pi);

    if (ok == 0)
    {
        displayWin32ApiError("CreateProcessWithLogon");
        return 1;
    }
    fwprintf(stderr, L"Done\n");
    return 0;
}