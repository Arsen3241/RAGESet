//ver 2.6.07-04-2025
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <tlhelp32.h>
#include <richedit.h>
#include <shlobj.h>
#include <algorithm>


std::string gtaBasePath = "";

std::string rageMpPath = "";

HBITMAP hBitmap = NULL;

std::string GetConfigFilePath() {
    char appPath[MAX_PATH];
    GetModuleFileNameA(NULL, appPath, MAX_PATH);
    std::string path = appPath;
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos + 1);
    }
    return path + "config.txt";
}


void SaveConfiguration(const std::string selectedPaths[5]) {
    std::string configPath = GetConfigFilePath();
    FILE* file = fopen(configPath.c_str(), "w");
    if (file) {

        fprintf(file, "GTA_BASE_PATH=%s\n", gtaBasePath.c_str());


        fprintf(file, "RAGE_MP_PATH=%s\n", rageMpPath.c_str());


        for (int i = 0; i < 5; i++) {
            fprintf(file, "%s\n", selectedPaths[i].c_str());
        }
        fclose(file);
    }
}


void LoadConfiguration(std::string selectedPaths[5]) {
    std::string configPath = GetConfigFilePath();
    FILE* file = fopen(configPath.c_str(), "r");
    if (file) {
        char buffer[MAX_PATH];
        int index = 0;

        while (fgets(buffer, MAX_PATH, file) && index <= 5) {

            size_t len = strlen(buffer);
            if (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
                buffer[len-1] = 0;
            }
            if (len > 1 && (buffer[len-2] == '\r')) {
                buffer[len-2] = 0;
            }


            if (strncmp(buffer, "GTA_BASE_PATH=", 14) == 0) {
                gtaBasePath = buffer + 14;
            }

            else if (strncmp(buffer, "RAGE_MP_PATH=", 13) == 0) {
                rageMpPath = buffer + 13;
            }
            else {

                if (buffer[0] != '\0' && GetFileAttributesA(buffer) != INVALID_FILE_ATTRIBUTES) {
                    selectedPaths[index] = buffer;
                }
                index++;
            }
        }
        fclose(file);
    }
}


HANDLE g_ThreadHandle = NULL;
bool g_ThreadShouldExit = false;


struct ThreadData {
    std::string paths[5];
    std::string basePath;
};


#define ID_RUN_BUTTON 101
#define ID_CONSOLE 102
#define ID_STATUS 103
#define ID_SELECT_GTA_PATH 104
#define ID_SELECT_RAGE_PATH 105


#define APP_BG_COLOR RGB(40, 44, 52)
#define APP_HEADER_COLOR RGB(30, 34, 42)
#define APP_CONSOLE_BG RGB(30, 30, 30)
#define APP_CONSOLE_TEXT RGB(220, 220, 220)
#define APP_BUTTON_NORMAL RGB(97, 175, 239)
#define APP_BUTTON_HOVER RGB(126, 204, 252)
#define APP_BUTTON_PRESSED RGB(77, 155, 219)
#define APP_TITLE_COLOR RGB(220, 220, 220)
#define APP_OUTLINE_COLOR RGB(86, 90, 100)
#define APP_AREA_FILL RGB(50, 54, 62)
#define APP_SELECTED_OUTLINE_COLOR RGB(120, 220, 100)
#define APP_PATH_NOT_SET RGB(239, 83, 80)
#define APP_PATH_SET RGB(129, 199, 132)


RECT headRect   = { 50, 80, 100, 130 };
RECT bodyRect   = { 40, 140, 110, 240 };
RECT legsRect   = { 45, 250, 105, 330 };
RECT reduxRect  = { 130, 100, 210, 150 };
RECT gunRect    = { 130, 160, 210, 210 };


std::string selectedPaths[5];
HWND hRunButton;
HWND hConsole;
HWND hStatus;
HWND hSelectGtaPath;
HWND hSelectRagePath;
HBRUSH hConsoleBrush;
HBRUSH hButtonBrush;
HFONT hConsoleFont;
HFONT hTitleFont;
bool isButtonHovered = false;
bool isPathButtonHovered = false;
bool isRagePathButtonHovered = false;


void AddConsoleText(const std::string& text) {
    if (hConsole) {
        int length = GetWindowTextLengthA(hConsole);
        SendMessageA(hConsole, EM_SETSEL, (WPARAM)length, (LPARAM)length);
        SendMessageA(hConsole, EM_REPLACESEL, FALSE, (LPARAM)(text + "\r\n").c_str());
        SendMessageA(hConsole, EM_SCROLLCARET, 0, 0);
    }
}


bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}


bool ProcessExists(const char* processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, processName) == 0) {
                CloseHandle(snapshot);
                return true;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return false;
}


std::string SelectFolderDialog(HWND hwnd) {
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.lpszTitle = "Select GTA V folder:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);

    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }

    return "";
}


bool IsShortcutFile(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string extension = filePath.substr(dotPos);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return (extension == ".lnk");
    }
    return false;
}


std::string SelectFilePath(HWND hwnd, const char* title) {
    OPENFILENAMEA ofn;
    CHAR szFile[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files\0*.*\0Shortcuts (*.lnk)\0*.lnk\0Executable (*.exe)\0*.exe\0";
    ofn.nFilterIndex = 2;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
}


void CopyFileToPath(const std::string& src, const std::string& destinationPath) {

    if (gtaBasePath.empty()) {
        AddConsoleText("Error: GTA path not set");
        return;
    }


    std::string fullDstPath = gtaBasePath + "\\" + destinationPath;


    size_t pos = src.find_last_of("\\/");
    std::string fileName = (pos != std::string::npos) ? src.substr(pos + 1) : src;


    bool isFullPath = (destinationPath.find(".rpf") != std::string::npos);


    std::string destDir = fullDstPath;
    if (!isFullPath) {

        CreateDirectoryA(destDir.c_str(), NULL);


        if (destDir.back() != '\\' && destDir.back() != '/') {
            destDir += '\\';
        }
        destDir += fileName;
    }


    AddConsoleText("Copying to: " + fullDstPath);
    if (CopyFileA(src.c_str(), fullDstPath.c_str(), FALSE)) {
        AddConsoleText("File copied successfully");
    } else {
        AddConsoleText("Error copying file: " + std::to_string(GetLastError()));
    }
}


std::string ShowFileDialog(HWND hwnd) {
    OPENFILENAMEA ofn;
    CHAR szFile[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hwnd;
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
}


void DrawButton(HWND hwnd, HDC hdc, RECT* buttonRect, const char* text, bool isHovered, bool isPressed, bool hasGreenOutline = false, bool isPathNotSet = false) {

    COLORREF bgColor;

    if (isPathNotSet) {
        bgColor = APP_PATH_NOT_SET;
    } else if (hasGreenOutline) {
        bgColor = APP_PATH_SET;
    } else {
        bgColor = isPressed ? APP_BUTTON_PRESSED :
                  (isHovered ? APP_BUTTON_HOVER : APP_BUTTON_NORMAL);
    }


    RECT rect = *buttonRect;
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);


    COLORREF outlineColor = hasGreenOutline ? APP_SELECTED_OUTLINE_COLOR : RGB(60, 60, 70);
    HPEN pen = CreatePen(PS_SOLID, hasGreenOutline ? 2 : 1, outlineColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);


    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HFONT hFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);


    DrawTextA(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}


void DrawArea(HDC hdc, RECT* rect, bool isSelected, const char* areaText = NULL) {

    HBRUSH bgBrush = CreateSolidBrush(APP_AREA_FILL);
    FillRect(hdc, rect, bgBrush);
    DeleteObject(bgBrush);


    COLORREF borderColor = isSelected ? APP_SELECTED_OUTLINE_COLOR : APP_OUTLINE_COLOR;
    HPEN pen = CreatePen(PS_SOLID, isSelected ? 2 : 1, borderColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, 8, 8);


    if (areaText == NULL) {
        int centerX = (rect->left + rect->right) / 2;
        int centerY = (rect->top + rect->bottom) / 2;
        int size = 10;


        MoveToEx(hdc, centerX - size, centerY - size, NULL);
        LineTo(hdc, centerX + size, centerY + size);

        MoveToEx(hdc, centerX - size, centerY + size, NULL);
        LineTo(hdc, centerX + size, centerY - size);
    }


    if (areaText != NULL) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);

        RECT textRect = *rect;
        DrawTextA(hdc, areaText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}


DWORD WINAPI ProcessWaitThread(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;


    while (!ProcessExists("EACLauncher.exe") && !g_ThreadShouldExit) {
        Sleep(1000);
    }

    if (!g_ThreadShouldExit) {

        if (IsWindow(hStatus)) {
            SetWindowTextA(hStatus, "All operations completed");
        }


        if (!data->paths[0].empty()) {
            CopyFileToPath(data->paths[0], "update\\");
        }
        if (!data->paths[1].empty()) {
            CopyFileToPath(data->paths[1], "update\\x64\\dlcpacks\\patchday18ng\\");
        }
        if (!data->paths[2].empty()) {
            CopyFileToPath(data->paths[2], "update\\x64\\dlcpacks\\mpbiker\\");
        }
        if (!data->paths[3].empty()) {
            CopyFileToPath(data->paths[3], "update\\update.rpf");
        }
        if (!data->paths[4].empty()) {
            CopyFileToPath(data->paths[4], "update\\x64\\dlcpacks\\patchday18ng\\dlc.rpf");
        }
    }

    delete data;
    return 0;
}


void RunOperation(HWND hwnd) {

    if (selectedPaths[0].empty() && selectedPaths[1].empty() &&
        selectedPaths[2].empty() && selectedPaths[3].empty() &&
        selectedPaths[4].empty()) {
        MessageBoxA(hwnd, "Please select at least one file!", "Warning", MB_ICONWARNING);
        return;
    }


    if (gtaBasePath.empty()) {
        MessageBoxA(hwnd, "Please select GTA V folder first!", "Warning", MB_ICONWARNING);
        return;
    }


    if (rageMpPath.empty()) {
        MessageBoxA(hwnd, "Please select RAGE MP first!", "Error", MB_ICONERROR);
        return;
    }


    EnableWindow(hRunButton, FALSE);
    InvalidateRect(hwnd, NULL, TRUE);


    bool isShortcut = IsShortcutFile(rageMpPath);
    AddConsoleText("Launching " + std::string(isShortcut ? "shortcut" : "executable") + ": " + rageMpPath);


    ShellExecuteA(NULL, "open", rageMpPath.c_str(), NULL, NULL, SW_SHOWNORMAL);


    RECT rcClient;
    GetClientRect(hwnd, &rcClient);


    int statusY = rcClient.bottom - 30;


    hStatus = CreateWindowA("STATIC", "Waiting for process...",
                              WS_CHILD | WS_VISIBLE | SS_CENTER,
                              0, statusY, rcClient.right, 20,
                              hwnd, (HMENU)ID_STATUS, GetModuleHandle(NULL), NULL);
    SendMessageA(hStatus, WM_SETFONT, (WPARAM)hTitleFont, TRUE);


    AddConsoleText("=== Operation Started ===");
    AddConsoleText("1. Game launcher started");
    AddConsoleText("2. Waiting for EACLauncher process...");
    AddConsoleText("3. Files will be copied automatically when process is detected");
    AddConsoleText("4. Using GTA: " + gtaBasePath);
    AddConsoleText("5. Using RAGE MP: " + rageMpPath);
    AddConsoleText("Note: You can close this window - files will be copied in background");


    ThreadData* data = new ThreadData();
    for (int i = 0; i < 5; i++) {
        data->paths[i] = selectedPaths[i];
    }
    data->basePath = gtaBasePath;


    g_ThreadShouldExit = false;


    g_ThreadHandle = CreateThread(NULL, 0, ProcessWaitThread, data, 0, NULL);


    SetWindowTextA(hStatus, "All operations completed");
    AddConsoleText("Background process started successfully");
    EnableWindow(hRunButton, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {

        hConsoleFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  FIXED_PITCH | FF_MODERN, "Consolas");

        hTitleFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");


        hConsoleBrush = CreateSolidBrush(APP_CONSOLE_BG);
        hButtonBrush = CreateSolidBrush(APP_BUTTON_NORMAL);



        hSelectRagePath = CreateWindowA("BUTTON", "Select RAGE",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   230, 400, 160, 40,
                                   hwnd, (HMENU)ID_SELECT_RAGE_PATH, GetModuleHandle(NULL), NULL);


        hRunButton = CreateWindowA("BUTTON", "Run",
                                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                400, 400, 100, 40,
                                hwnd, (HMENU)ID_RUN_BUTTON, GetModuleHandle(NULL), NULL);


        hSelectGtaPath = CreateWindowA("BUTTON", "Select GTA",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   510, 400, 160, 40,
                                   hwnd, (HMENU)ID_SELECT_GTA_PATH, GetModuleHandle(NULL), NULL);


        hConsole = CreateWindowA("EDIT", "",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                              230, 110, 400, 250,
                              hwnd, (HMENU)ID_CONSOLE, GetModuleHandle(NULL), NULL);


        SendMessageA(hConsole, WM_SETFONT, (WPARAM)hConsoleFont, TRUE);


        SetWindowLongA(hConsole, GWL_EXSTYLE, GetWindowLongA(hConsole, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);


        AddConsoleText("=== GTA5RP Editor ===");
        AddConsoleText("Application started successfully");
        AddConsoleText("");
        AddConsoleText("Instructions:");
        AddConsoleText("1. Click on 'Select GTA' to select GTA V folder (required)");
        AddConsoleText("2. Click on 'Select RAGE' to select RAGE MP executable (required)");
        AddConsoleText("3. Click on model areas to select files");
        AddConsoleText("4. Press Run to start the game");
        AddConsoleText("5. Files will be copied automatically");


        AddConsoleText("");
        AddConsoleText("Loaded configuration:");
        if (gtaBasePath.empty()) {
            AddConsoleText("GTA: Not set (required)");
        } else {
            AddConsoleText("GTA path: " + gtaBasePath);
        }
        if (rageMpPath.empty()) {
            AddConsoleText("RAGE MP: Not set (required)");
        } else {
            AddConsoleText("RAGE MP path: " + rageMpPath);
        }
        if (!selectedPaths[0].empty()) AddConsoleText("Head file: " + selectedPaths[0]);
        if (!selectedPaths[1].empty()) AddConsoleText("Body file: " + selectedPaths[1]);
        if (!selectedPaths[2].empty()) AddConsoleText("Legs file: " + selectedPaths[2]);
        if (!selectedPaths[3].empty()) AddConsoleText("Redux file: " + selectedPaths[3]);
        if (!selectedPaths[4].empty()) AddConsoleText("Gunpack file: " + selectedPaths[4]);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_RUN_BUTTON) {
            RunOperation(hwnd);
        } else if (LOWORD(wParam) == ID_SELECT_GTA_PATH) {
            std::string newPath = SelectFolderDialog(hwnd);
            if (!newPath.empty()) {
                gtaBasePath = newPath;
                AddConsoleText("GTA path set to: " + gtaBasePath);
                SaveConfiguration(selectedPaths);
                InvalidateRect(hSelectGtaPath, NULL, TRUE);
            }
        } else if (LOWORD(wParam) == ID_SELECT_RAGE_PATH) {
            std::string newPath = SelectFilePath(hwnd, "Select RAGE MP");
            if (!newPath.empty()) {
                rageMpPath = newPath;
                bool isShortcut = IsShortcutFile(newPath);
                AddConsoleText("RAGE MP path set to: " + rageMpPath);
                SaveConfiguration(selectedPaths);
                InvalidateRect(hSelectRagePath, NULL, TRUE);
            }
        }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == ID_RUN_BUTTON) {
            DrawButton(hwnd, pDIS->hDC, &pDIS->rcItem, "Run",
                      isButtonHovered, (pDIS->itemState & ODS_SELECTED));
            return TRUE;
        } else if (pDIS->CtlID == ID_SELECT_GTA_PATH) {

            bool hasValidPath = !gtaBasePath.empty();
            DrawButton(hwnd, pDIS->hDC, &pDIS->rcItem, "Select GTA",
                      isPathButtonHovered, (pDIS->itemState & ODS_SELECTED),
                      hasValidPath, !hasValidPath);
            return TRUE;
        } else if (pDIS->CtlID == ID_SELECT_RAGE_PATH) {

            bool hasValidPath = !rageMpPath.empty();
            DrawButton(hwnd, pDIS->hDC, &pDIS->rcItem, "Select RAGE",
                      isRagePathButtonHovered, (pDIS->itemState & ODS_SELECTED),
                      hasValidPath, !hasValidPath);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
        if ((HWND)lParam == hConsole) {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, APP_CONSOLE_TEXT);
            SetBkColor(hdcEdit, APP_CONSOLE_BG);
            return (LRESULT)hConsoleBrush;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);
        POINT pt = { xPos, yPos };


        RECT buttonRect, pathButtonRect, ragePathButtonRect;
        GetWindowRect(hRunButton, &buttonRect);
        GetWindowRect(hSelectGtaPath, &pathButtonRect);
        GetWindowRect(hSelectRagePath, &ragePathButtonRect);
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);

        bool wasHovered = isButtonHovered;
        bool wasPathHovered = isPathButtonHovered;
        bool wasRagePathHovered = isRagePathButtonHovered;

        isButtonHovered = PtInRect(&buttonRect, ptScreen);
        isPathButtonHovered = PtInRect(&pathButtonRect, ptScreen);
        isRagePathButtonHovered = PtInRect(&ragePathButtonRect, ptScreen);

        if (wasHovered != isButtonHovered) {
            InvalidateRect(hRunButton, NULL, TRUE);
        }

        if (wasPathHovered != isPathButtonHovered) {
            InvalidateRect(hSelectGtaPath, NULL, TRUE);
        }

        if (wasRagePathHovered != isRagePathButtonHovered) {
            InvalidateRect(hSelectRagePath, NULL, TRUE);
        }


        if ((!wasHovered && isButtonHovered) ||
            (!wasPathHovered && isPathButtonHovered) ||
            (!wasRagePathHovered && isRagePathButtonHovered)) {
            TRACKMOUSEEVENT tme = { sizeof(tme) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        bool redraw = false;

        if (isButtonHovered) {
            isButtonHovered = false;
            InvalidateRect(hRunButton, NULL, TRUE);
            redraw = true;
        }

        if (isPathButtonHovered) {
            isPathButtonHovered = false;
            InvalidateRect(hSelectGtaPath, NULL, TRUE);
            redraw = true;
        }

        if (isRagePathButtonHovered) {
            isRagePathButtonHovered = false;
            InvalidateRect(hSelectRagePath, NULL, TRUE);
            redraw = true;
        }

        if (redraw) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);
        POINT pt = { xPos, yPos };


        if (PtInRect(&headRect, pt)) {
            selectedPaths[0] = ShowFileDialog(hwnd);
            if (!selectedPaths[0].empty()) {
                AddConsoleText("Selected Head file: " + selectedPaths[0]);
                SaveConfiguration(selectedPaths);
            }
        } else if (PtInRect(&bodyRect, pt)) {
            selectedPaths[1] = ShowFileDialog(hwnd);
            if (!selectedPaths[1].empty()) {
                AddConsoleText("Selected Body file: " + selectedPaths[1]);
                SaveConfiguration(selectedPaths);
            }
        } else if (PtInRect(&legsRect, pt)) {
            selectedPaths[2] = ShowFileDialog(hwnd);
            if (!selectedPaths[2].empty()) {
                AddConsoleText("Selected Legs file: " + selectedPaths[2]);
                SaveConfiguration(selectedPaths);
            }
        } else if (PtInRect(&reduxRect, pt)) {
            selectedPaths[3] = ShowFileDialog(hwnd);
            if (!selectedPaths[3].empty()) {
                AddConsoleText("Selected Redux file: " + selectedPaths[3]);
                SaveConfiguration(selectedPaths);
            }
        } else if (PtInRect(&gunRect, pt)) {
            selectedPaths[4] = ShowFileDialog(hwnd);
            if (!selectedPaths[4].empty()) {
                AddConsoleText("Selected Gunpack file: " + selectedPaths[4]);
                SaveConfiguration(selectedPaths);
            }
        }

        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);


        RECT rcClient;
        GetClientRect(hwnd, &rcClient);


        HBRUSH bgBrush = CreateSolidBrush(APP_BG_COLOR);
        FillRect(hdc, &rcClient, bgBrush);
        DeleteObject(bgBrush);


        RECT headerRect = rcClient;
        headerRect.bottom = 50;
        HBRUSH headerBrush = CreateSolidBrush(APP_HEADER_COLOR);
        FillRect(hdc, &headerRect, headerBrush);
        DeleteObject(headerBrush);


        SetTextColor(hdc, APP_TITLE_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, hTitleFont);
        DrawTextA(hdc, "GTA5RP - Editor", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);


        RECT consoleHeader = {230, 80, 630, 105};
        DrawTextA(hdc, "Operations Console", -1, &consoleHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);


        DrawArea(hdc, &headRect, !selectedPaths[0].empty());
        DrawArea(hdc, &bodyRect, !selectedPaths[1].empty());
        DrawArea(hdc, &legsRect, !selectedPaths[2].empty());


        DrawArea(hdc, &reduxRect, !selectedPaths[3].empty(), "Redux");
        DrawArea(hdc, &gunRect, !selectedPaths[4].empty(), "Gunpack");

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_SIZE: {

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);


        MoveWindow(hConsole, 230, 110, rcClient.right - 250, rcClient.bottom - 200, TRUE);


        int totalButtonWidth = rcClient.right - 230;
        int buttonWidth = 100;
        int pathButtonWidth = 160;
        int spacing = 20;


        int centerX = rcClient.right / 2;
        int btnY = rcClient.bottom - 80;


        int runBtnX = centerX - buttonWidth / 2;
        int rageBtnX = runBtnX - spacing - pathButtonWidth;
        int gtaBtnX = runBtnX + buttonWidth + spacing;


        MoveWindow(hSelectRagePath, rageBtnX, btnY, pathButtonWidth, 40, TRUE);
        MoveWindow(hRunButton, runBtnX, btnY, buttonWidth, 40, TRUE);
        MoveWindow(hSelectGtaPath, gtaBtnX, btnY, pathButtonWidth, 40, TRUE);


        if (IsWindow(hStatus)) {
            MoveWindow(hStatus, 0, rcClient.bottom - 30, rcClient.right, 20, TRUE);
        }

        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_DESTROY:

        g_ThreadShouldExit = true;


        if (g_ThreadHandle != NULL) {
            WaitForSingleObject(g_ThreadHandle, 1000);
            CloseHandle(g_ThreadHandle);
            g_ThreadHandle = NULL;
        }


        DeleteObject(hConsoleBrush);
        DeleteObject(hButtonBrush);
        DeleteObject(hConsoleFont);
        DeleteObject(hTitleFont);

        AddConsoleText("Closing application...");
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {

    if (!IsRunAsAdmin()) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        SHELLEXECUTEINFOA sei = { sizeof(sei) };
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExA(&sei)) {
            return 0;
        } else {
            MessageBoxA(NULL, "Failed to get administrator rights", "Error", MB_ICONERROR);
            return 1;
        }
    }


    LoadConfiguration(selectedPaths);


    HICON hIcon = (HICON)LoadImageA(NULL, "mp.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);


    WNDCLASSEX wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(APP_BG_COLOR);
    wc.lpszClassName = "MyAppClass";
    wc.hIcon         = hIcon;
    wc.hIconSm       = hIcon;

    RegisterClassEx(&wc);


    HWND hwnd = CreateWindowExA(0, "MyAppClass", "RAGESet",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 650, 500,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
