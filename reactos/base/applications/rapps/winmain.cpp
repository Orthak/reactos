/*
 * PROJECT:         ReactOS Applications Manager
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            base/applications/rapps/winmain.cpp
 * PURPOSE:         Main program
 * PROGRAMMERS:     Dmitry Chapyshev           (dmitry@reactos.org)
 *                  Ismael Ferreras Morezuelas (swyterzone+ros@gmail.com)
 *                  Alexander Shaposhnikov     (chaez.san@gmail.com)
 */
#include "defines.h"

#include "rapps.h"

#include <atlbase.h>
#include <atlcom.h>
#include <shellapi.h>

HWND hMainWnd;
HINSTANCE hInst;
INT SelectedEnumType = ENUM_ALL_COMPONENTS;
SETTINGS_INFO SettingsInfo;

ATL::CStringW szSearchPattern;

class CRAppsModule : public CComModule
{
public:
};

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

CRAppsModule gModule;
CAtlWinModule gWinModule;

//void *operator new (size_t, void *buf)
//{
//    return buf;
//}

static VOID InitializeAtlModule(HINSTANCE hInstance, BOOL bInitialize)
{
    if (bInitialize)
    {
        gModule.Init(ObjectMap, hInstance, NULL);
    }
    else
    {
        gModule.Term();
    }
}

VOID
FillDefaultSettings(PSETTINGS_INFO pSettingsInfo)
{
    ATL::CStringW szDownloadDir;
    pSettingsInfo->bSaveWndPos = TRUE;
    pSettingsInfo->bUpdateAtStart = FALSE;
    pSettingsInfo->bLogEnabled = TRUE;

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, szDownloadDir.GetBuffer(MAX_PATH))))
    {
        szDownloadDir.ReleaseBuffer();
        if (!szDownloadDir.GetEnvironmentVariableW(L"SystemDrive"))
        {
            szDownloadDir = L"C:";
        }
    }
    else
        szDownloadDir.ReleaseBuffer();

    szDownloadDir += L"\\RAPPS Downloads";
    ATL::CStringW::CopyChars(pSettingsInfo->szDownloadDir, 
                             _countof(pSettingsInfo->szDownloadDir),
                             szDownloadDir.GetString(),
                             szDownloadDir.GetLength() + 1);

    pSettingsInfo->bDelInstaller = FALSE;
    pSettingsInfo->Maximized = FALSE;
    pSettingsInfo->Left = CW_USEDEFAULT;
    pSettingsInfo->Top = CW_USEDEFAULT;
    pSettingsInfo->Width = 680;
    pSettingsInfo->Height = 450;
    pSettingsInfo->Proxy = 0;

    pSettingsInfo->szProxyServer[0] = UNICODE_NULL;
    pSettingsInfo->szNoProxyFor[0] = UNICODE_NULL;
}

static BOOL
LoadSettings(VOID)
{
    HKEY hKey;
    DWORD dwSize;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ReactOS\\rapps", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        dwSize = sizeof(SettingsInfo);
        if (RegQueryValueExW(hKey, L"Settings", NULL, NULL, (LPBYTE) &SettingsInfo, &dwSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return TRUE;
        }

        RegCloseKey(hKey);
    }

    return FALSE;
}

VOID
SaveSettings(HWND hwnd)
{
    WINDOWPLACEMENT wp;
    HKEY hKey;

    if (SettingsInfo.bSaveWndPos)
    {
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);

        SettingsInfo.Left = wp.rcNormalPosition.left;
        SettingsInfo.Top = wp.rcNormalPosition.top;
        SettingsInfo.Width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        SettingsInfo.Height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        SettingsInfo.Maximized = (wp.showCmd == SW_MAXIMIZE || (wp.showCmd == SW_SHOWMINIMIZED && (wp.flags & WPF_RESTORETOMAXIMIZED)));
    }

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ReactOS\\rapps", 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, L"Settings", 0, REG_BINARY, (LPBYTE) &SettingsInfo, sizeof(SettingsInfo));
        RegCloseKey(hKey);
    }
}

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    WCHAR szWindowClass[] = L"ROSAPPMGR";
    HANDLE hMutex = NULL;
    HACCEL KeyBrd;
    MSG Msg;

    InitializeAtlModule(hInstance, TRUE);

    switch (GetUserDefaultUILanguage())
    {
    case MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT):
        SetProcessDefaultLayout(LAYOUT_RTL);
        break;

    default:
        break;
    }

    hInst = hInstance;

    hMutex = CreateMutexW(NULL, FALSE, szWindowClass);
    if ((!hMutex) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        /* If already started, it is found its window */
        HWND hWindow = FindWindowW(szWindowClass, NULL);

        /* Activate window */
        ShowWindow(hWindow, SW_SHOWNORMAL);
        SetForegroundWindow(hWindow);
        return 1;
    }

    if (!LoadSettings())
    {
        FillDefaultSettings(&SettingsInfo);
    }

    InitLogs();

    InitCommonControls();

    hMainWnd = CreateMainWindow();
    if (!hMainWnd) goto Exit;

    /* Maximize it if we must */
    ShowWindow(hMainWnd, (SettingsInfo.bSaveWndPos && SettingsInfo.Maximized ? SW_MAXIMIZE : nShowCmd));
    UpdateWindow(hMainWnd);

    //TODO: get around the ugliness
    if (SettingsInfo.bUpdateAtStart)
        GetAvailableApps()->UpdateAppsDB();

    /* Load the menu hotkeys */
    KeyBrd = LoadAcceleratorsW(NULL, MAKEINTRESOURCEW(HOTKEYS));

    /* Message Loop */
    while (GetMessageW(&Msg, NULL, 0, 0))
    {
        if (!TranslateAcceleratorW(hMainWnd, KeyBrd, &Msg))
        {
            TranslateMessage(&Msg);
            DispatchMessageW(&Msg);
        }
    }

Exit:
    if (hMutex)
        CloseHandle(hMutex);

    InitializeAtlModule(hInstance, FALSE);

    return 0;
}
