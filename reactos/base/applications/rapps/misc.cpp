/*
 * PROJECT:         ReactOS Applications Manager
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            base/applications/rapps/misc.cpp
 * PURPOSE:         Misc functions
 * PROGRAMMERS:     Dmitry Chapyshev           (dmitry@reactos.org)
 *                  Ismael Ferreras Morezuelas (swyterzone+ros@gmail.com)
 *                  Alexander Shaposhnikov     (chaez.san@gmail.com)
 */
#include "defines.h"

#include "gui.h"
#include "misc.h"

 /* SESSION Operation */
#define EXTRACT_FILLFILELIST  0x00000001
#define EXTRACT_EXTRACTFILES  0x00000002

static HANDLE hLog = NULL;

typedef struct
{
    int erfOper;
    int erfType;
    BOOL fError;
} ERF, *PERF;

struct FILELIST
{
    LPSTR FileName;
    FILELIST *next;
    BOOL DoExtract;
};

struct SESSION
{
    INT FileSize;
    ERF Error;
    FILELIST *FileList;
    INT FileCount;
    INT Operation;
    CHAR Destination[MAX_PATH];
    CHAR CurrentFile[MAX_PATH];
    CHAR Reserved[MAX_PATH];
    FILELIST *FilterList;
};

typedef HRESULT(WINAPI *fnExtract)(SESSION *dest, LPCSTR szCabName);
fnExtract pfnExtract;


int
GetWindowWidth(HWND hwnd)
{
    RECT Rect;

    GetWindowRect(hwnd, &Rect);
    return (Rect.right - Rect.left);
}

int
GetWindowHeight(HWND hwnd)
{
    RECT Rect;

    GetWindowRect(hwnd, &Rect);
    return (Rect.bottom - Rect.top);
}

int
GetClientWindowWidth(HWND hwnd)
{
    RECT Rect;

    GetClientRect(hwnd, &Rect);
    return (Rect.right - Rect.left);
}

int
GetClientWindowHeight(HWND hwnd)
{
    RECT Rect;

    GetClientRect(hwnd, &Rect);
    return (Rect.bottom - Rect.top);
}

VOID
CopyTextToClipboard(LPCWSTR lpszText)
{
    HRESULT hr;

    if (OpenClipboard(NULL))
    {
        HGLOBAL ClipBuffer;
        WCHAR *Buffer;
        DWORD cchBuffer;

        EmptyClipboard();
        cchBuffer = wcslen(lpszText) + 1;
        ClipBuffer = GlobalAlloc(GMEM_DDESHARE, cchBuffer * sizeof(WCHAR));
        Buffer = (PWCHAR) GlobalLock(ClipBuffer);
        hr = StringCchCopyW(Buffer, cchBuffer, lpszText);
        GlobalUnlock(ClipBuffer);

        if (SUCCEEDED(hr))
            SetClipboardData(CF_UNICODETEXT, ClipBuffer);

        CloseClipboard();
    }
}

VOID
SetWelcomeText(VOID)
{
    ATL::CStringW szText;

    szText.LoadStringW(hInst, IDS_WELCOME_TITLE);
    NewRichEditText(szText, CFE_BOLD);

    szText.LoadStringW(hInst, IDS_WELCOME_TEXT);
    InsertRichEditText(szText, 0);

    szText.LoadStringW(hInst, IDS_WELCOME_URL);
    InsertRichEditText(szText, CFM_LINK);
}

VOID
ShowPopupMenu(HWND hwnd, UINT MenuID, UINT DefaultItem)
{
    HMENU hMenu = NULL;
    HMENU hPopupMenu;
    MENUITEMINFO mii;
    POINT pt;

    if (MenuID)
    {
        hMenu = LoadMenuW(hInst, MAKEINTRESOURCEW(MenuID));
        hPopupMenu = GetSubMenu(hMenu, 0);
    }
    else
        hPopupMenu = GetMenu(hwnd);

    ZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    GetMenuItemInfoW(hPopupMenu, DefaultItem, FALSE, &mii);

    if (!(mii.fState & MFS_GRAYED))
        SetMenuDefaultItem(hPopupMenu, DefaultItem, FALSE);

    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hPopupMenu, 0, pt.x, pt.y, 0, hMainWnd, NULL);

    if (hMenu)
        DestroyMenu(hMenu);
}

BOOL
StartProcess(ATL::CStringW &Path, BOOL Wait)
{
    BOOL result = StartProcess(const_cast<LPWSTR>(Path.GetString()), Wait);
    return result;
}

BOOL
StartProcess(LPWSTR lpPath, BOOL Wait)
{
    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    DWORD dwRet;
    MSG msg;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    if (!CreateProcessW(NULL, lpPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        return FALSE;
    }

    CloseHandle(pi.hThread);
    if (Wait) EnableWindow(hMainWnd, FALSE);

    while (Wait)
    {
        dwRet = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLEVENTS);
        if (dwRet == WAIT_OBJECT_0 + 1)
        {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (dwRet == WAIT_OBJECT_0 || dwRet == WAIT_FAILED)
                break;
        }
    }

    CloseHandle(pi.hProcess);

    if (Wait)
    {
        EnableWindow(hMainWnd, TRUE);
        SetForegroundWindow(hMainWnd);
        SetFocus(hMainWnd);
    }

    return TRUE;
}

BOOL
GetStorageDirectory(ATL::CStringW& Directory)
{
    if (!SHGetSpecialFolderPathW(NULL, Directory.GetBuffer(MAX_PATH), CSIDL_LOCAL_APPDATA, TRUE))
    {
        Directory.ReleaseBuffer();
        return FALSE;
    }

    Directory.ReleaseBuffer();
    Directory += L"\\rapps";

    return (CreateDirectoryW(Directory.GetString(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
}

BOOL
ExtractFilesFromCab(const ATL::CStringW &CabName, const ATL::CStringW &OutputPath)
{
    return ExtractFilesFromCab(CabName.GetString(), OutputPath.GetString());
}

BOOL
ExtractFilesFromCab(LPCWSTR lpCabName, LPCWSTR lpOutputPath)
{
    HINSTANCE hCabinetDll;
    CHAR szCabName[MAX_PATH];
    SESSION Dest;
    HRESULT Result;

    hCabinetDll = LoadLibraryW(L"cabinet.dll");
    if (hCabinetDll)
    {
        pfnExtract = (fnExtract) GetProcAddress(hCabinetDll, "Extract");
        if (pfnExtract)
        {
            ZeroMemory(&Dest, sizeof(Dest));

            WideCharToMultiByte(CP_ACP, 0, lpOutputPath, -1, Dest.Destination, MAX_PATH, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, lpCabName, -1, szCabName, MAX_PATH, NULL, NULL);
            Dest.Operation = EXTRACT_FILLFILELIST;

            Result = pfnExtract(&Dest, szCabName);
            if (Result == S_OK)
            {
                Dest.Operation = EXTRACT_EXTRACTFILES;
                Result = pfnExtract(&Dest, szCabName);
                if (Result == S_OK)
                {
                    FreeLibrary(hCabinetDll);
                    return TRUE;
                }
            }
        }
        FreeLibrary(hCabinetDll);
    }

    return FALSE;
}

VOID
InitLogs(VOID)
{
    WCHAR szBuf[MAX_PATH] = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\ReactOS Application Manager";
    WCHAR szPath[MAX_PATH];
    DWORD dwCategoryNum = 1;
    DWORD dwDisp, dwData;
    HKEY hKey;

    if (!SettingsInfo.bLogEnabled) return;

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        szBuf, 0, NULL,
                        REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &hKey, &dwDisp) != ERROR_SUCCESS)
    {
        return;
    }

    if (!GetModuleFileNameW(NULL, szPath, _countof(szPath)))
        return;

    if (RegSetValueExW(hKey,
                       L"EventMessageFile",
                       0,
                       REG_EXPAND_SZ,
                       (LPBYTE) szPath,
                       (DWORD) (wcslen(szPath) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return;
    }

    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
        EVENTLOG_INFORMATION_TYPE;

    if (RegSetValueExW(hKey,
                       L"TypesSupported",
                       0,
                       REG_DWORD,
                       (LPBYTE) &dwData,
                       sizeof(DWORD)) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return;
    }

    if (RegSetValueExW(hKey,
                       L"CategoryMessageFile",
                       0,
                       REG_EXPAND_SZ,
                       (LPBYTE) szPath,
                       (DWORD) (wcslen(szPath) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return;
    }

    if (RegSetValueExW(hKey,
                       L"CategoryCount",
                       0,
                       REG_DWORD,
                       (LPBYTE) &dwCategoryNum,
                       sizeof(DWORD)) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return;
    }

    RegCloseKey(hKey);

    hLog = RegisterEventSourceW(NULL, L"ReactOS Application Manager");
}


VOID
FreeLogs(VOID)
{
    if (hLog) DeregisterEventSource(hLog);
}


BOOL
WriteLogMessage(WORD wType, DWORD dwEventID, LPCWSTR lpMsg)
{
    if (!SettingsInfo.bLogEnabled) return TRUE;

    if (!ReportEventW(hLog, wType, 0, dwEventID,
                      NULL, 1, 0, &lpMsg, NULL))
    {
        return FALSE;
    }

    return TRUE;
}

BOOL
GetInstalledVersion_WowUser(_Out_opt_ ATL::CStringW* szVersionResult,
                            _In_z_ const ATL::CStringW& RegName,
                            _In_ BOOL IsUserKey,
                            _In_ REGSAM keyWow)
{
    HKEY hKey;
    BOOL bHasSucceded = FALSE;
    ATL::CStringW szVersion;
    ATL::CStringW szPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + RegName;

    if (RegOpenKeyExW(IsUserKey ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
                      szPath.GetString(), 0, keyWow | KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        if (szVersionResult != NULL)
        {
            DWORD dwSize = MAX_PATH * sizeof(WCHAR);
            DWORD dwType = REG_SZ;
            if (RegQueryValueExW(hKey,
                                 L"DisplayVersion",
                                 NULL,
                                 &dwType,
                                 (LPBYTE) szVersion.GetBuffer(MAX_PATH),
                                 &dwSize) == ERROR_SUCCESS)
            {
                szVersion.ReleaseBuffer();
                *szVersionResult = szVersion;
                bHasSucceded = TRUE;
            }
            else
            {
                szVersion.ReleaseBuffer();
            }
        }
        else
        {
            bHasSucceded = TRUE;
            szVersion.ReleaseBuffer();
        }

    }

    RegCloseKey(hKey);
    return bHasSucceded;
}

BOOL GetInstalledVersion(ATL::CStringW * pszVersion, const ATL::CStringW & szRegName)
{
    return (!szRegName.IsEmpty()
            && (GetInstalledVersion_WowUser(pszVersion, szRegName, TRUE, KEY_WOW64_32KEY)
                || GetInstalledVersion_WowUser(pszVersion, szRegName, FALSE, KEY_WOW64_32KEY)
                || GetInstalledVersion_WowUser(pszVersion, szRegName, TRUE, KEY_WOW64_64KEY)
                || GetInstalledVersion_WowUser(pszVersion, szRegName, FALSE, KEY_WOW64_64KEY)));
}
