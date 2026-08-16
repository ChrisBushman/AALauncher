/* Minimal native Win9x launcher for Amulets & Armor.
 *
 * AALauncher's real Windows build (../AALauncher/AALauncher.sln) is a
 * .NET/C++-CLI project -- the .NET CLR itself requires Windows 7+ at
 * minimum, so it cannot run on 95/98/ME at all regardless of toolchain.
 * This is a separate, dependency-free replacement for that platform
 * only: plain Win32 (CreateWindowEx/BUTTON/EDIT/STATIC controls, all
 * present since Windows 95), no MFC, no Qt, no .NET, no SDL -- just
 * user32/gdi32/kernel32, the same three DLLs every real Windows install
 * already has.
 *
 * Deliberately narrower in scope than the Qt launcher (mac/MainWindow.cpp)
 * or the IRIX one (irix/main.c): no embedded web view, no LAN server
 * discovery, no known-servers list, no in-app Script Compiler window --
 * just the three actions that actually get you playing (start server /
 * play single-player / play network), matching the same same-directory
 * binary lookup convention (AA.exe/AAServer.exe expected alongside this
 * launcher) every other platform's launcher already uses.
 *
 * Display/Color Depth/Level of Detail settings write directly to
 * resolution.ini (read by AA.exe's RESSCALE module, see
 * Source/Win32/RESSCALE.C) before launching AA.exe -- AAServer.exe
 * doesn't consume that file, so Start Server skips it. Uses plain
 * BS_RADIOBUTTON (not BS_AUTORADIOBUTTON) with explicit
 * CheckRadioButton() calls rather than relying on WS_GROUP-based
 * auto-exclusivity, since GROUPBOX controls (also real BUTTON-class
 * windows) sitting between radio sets in tab order make that automatic
 * behavior easy to get subtly wrong -- CheckRadioButton is simple and
 * unambiguous instead.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_EDIT_HOST    101
#define ID_EDIT_PORT    102
#define ID_BTN_SERVER   201
#define ID_BTN_SINGLE   202
#define ID_BTN_NETWORK  203

#define ID_RADIO_WINDOWED    301
#define ID_RADIO_FULLSCREEN  302

#define ID_RADIO_BPP_AUTO    311
#define ID_RADIO_BPP_8       312
#define ID_RADIO_BPP_16      313
#define ID_RADIO_BPP_24      314
#define ID_RADIO_BPP_32      315

/* 1-6, matching HIGHRES_MAX_SCALE/VIEW3D_SCALE_MAX (see their own
   comments in Include/HIGHRES.H and Include/3D_VIEW.H). Was briefly
   capped at 3 chasing a stack/.bss-growth theory for a real-hardware
   crash that turned out to be an unrelated HighRes frame-buffer stride
   bug (fixed at the allocation site in HIGHRES.C) -- restored once the
   actual cause was found and fixed there instead. */
#define ID_RADIO_DETAIL_1    321
#define ID_RADIO_DETAIL_2    322
#define ID_RADIO_DETAIL_3    323
#define ID_RADIO_DETAIL_4    324
#define ID_RADIO_DETAIL_5    325
#define ID_RADIO_DETAIL_6    326

static HWND g_hostEdit, g_portEdit;
static char g_selfDir[MAX_PATH];

/* Same-directory binary lookup, matching DefaultBinary() in every other
   platform's launcher (irix/main.c, mac/MainWindow.cpp, ppc/AppDelegate.m). */
static void GetSelfDir(void)
{
    char path[MAX_PATH];
    char *lastSlash;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    lastSlash = strrchr(path, '\\');
    if (lastSlash != NULL) {
        *lastSlash = '\0';
    }
    strncpy(g_selfDir, path, MAX_PATH - 1);
    g_selfDir[MAX_PATH - 1] = '\0';
}

static void ShowLaunchError(const char *what)
{
    char msg[MAX_PATH + 64];
    wsprintfA(msg, "Failed to launch %s.", what);
    MessageBoxA(NULL, msg, "Launch Error", MB_ICONERROR | MB_OK);
}

/* Builds "path\to\exe.exe" [arg1] [arg2] as a single command line and
   starts it via CreateProcess -- the real Win32 process-launch API,
   present since Windows 95, taking the place of every other platform's
   fork()+execv(). */
static void LaunchProcess(const char *exeName, const char *arg1, const char *arg2)
{
    char exePath[MAX_PATH];
    char cmdLine[MAX_PATH * 2];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    wsprintfA(exePath, "%s\\%s", g_selfDir, exeName);

    wsprintfA(cmdLine, "\"%s\"", exePath);
    if (arg1 != NULL && arg1[0] != '\0') {
        lstrcatA(cmdLine, " ");
        lstrcatA(cmdLine, arg1);
    }
    if (arg2 != NULL && arg2[0] != '\0') {
        lstrcatA(cmdLine, " ");
        lstrcatA(cmdLine, arg2);
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(exePath, cmdLine, NULL, NULL, FALSE, 0, NULL, g_selfDir, &si, &pi)) {
        ShowLaunchError(exeName);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static void GetEditText(HWND edit, char *buf, int bufLen)
{
    GetWindowTextA(edit, buf, bufLen);
}

/* Reads "key=value" from resolution.ini in the current directory (a plain
   int value, matching every key RESSCALE.C's own IReadIni understands).
   Returns fallbackVal if the file or key doesn't exist. Simpler than
   RESSCALE.C's own parser (exact-case key match, no comment stripping)
   since this file is only ever written by AA.exe or this launcher
   itself, both of which always emit "key=value" verbatim. */
static int ReadIniInt(const char *key, int fallbackVal)
{
    FILE *fp;
    char line[256];
    size_t keyLen = strlen(key);

    fp = fopen("resolution.ini", "r");
    if (fp == NULL)
        return fallbackVal;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if ((strncmp(line, key, keyLen) == 0) && (line[keyLen] == '=')) {
            fclose(fp);
            return atoi(line + keyLen + 1);
        }
    }
    fclose(fp);
    return fallbackVal;
}

/* Rewrites resolution.ini with the given fullscreen/bpp/detail values,
   preserving every other line (comments, scale=/width=/height=/fit=/
   aspect=/hotkeys=) exactly as found. Any prior fullscreen=/bpp=/
   detail= line is dropped and a fresh one appended for each at the end
   -- simpler and safer than in-place value substitution, and the exact
   position of these keys in the file doesn't matter to RESSCALE.C's own
   reader. If resolution.ini doesn't exist yet, this just creates a
   minimal file; AA.exe's own RESSCALE module fills in the rest (scale=/
   fit=/aspect=/hotkeys=) with its compiled-in defaults the first time it
   runs (the same defaults it would have written into a fresh file
   itself).

   width/height are managed here too, not left to AA.exe's own defaults,
   because RESSCALE.C's IComputeWindowSize() gives an explicit width=/
   height= absolute priority over scale= -- so leaving a stale explicit
   size in the file (e.g. a prior run's) would silently pin the window at
   that size no matter what detail level is picked here, decoupling
   "Level of Detail" from the actual window size in a way that surprised
   real-hardware testing (picking a lower/faster detail level kept
   defaulting to the same large window the higher levels used). Tying
   window size to detail level directly restores the old behaviour where
   a lower/faster setting also meant a smaller window -- and a smaller
   window is a real per-frame cost of its own (more pixels to stretch-blit
   every frame), not just a cosmetic default. */
static void WriteResolutionIni(int fullscreen, int bpp, int detail, int width, int height)
{
    static char oldBuf[8192];
    static char newBuf[8192];
    FILE *fp;
    size_t oldLen = 0;
    size_t newLen = 0;
    char *p;

    oldBuf[0] = '\0';
    fp = fopen("resolution.ini", "rb");
    if (fp != NULL) {
        oldLen = fread(oldBuf, 1, sizeof(oldBuf) - 1, fp);
        oldBuf[oldLen] = '\0';
        fclose(fp);
    }

    newBuf[0] = '\0';
    p = oldBuf;
    while (*p != '\0') {
        char *lineEnd = strchr(p, '\n');
        size_t lineLen;
        int isManaged;

        lineLen = (lineEnd != NULL) ? ((size_t)(lineEnd - p) + 1) : strlen(p);

        isManaged =
            (strncmp(p, "fullscreen=", 11) == 0) ||
            (strncmp(p, "bpp=", 4) == 0) ||
            (strncmp(p, "detail=", 7) == 0) ||
            (strncmp(p, "width=", 6) == 0) ||
            (strncmp(p, "height=", 7) == 0);

        if ((!isManaged) && ((newLen + lineLen) < sizeof(newBuf))) {
            memcpy(newBuf + newLen, p, lineLen);
            newLen += lineLen;
        }

        if (lineEnd == NULL)
            break;
        p = lineEnd + 1;
    }
    newBuf[newLen] = '\0';

    if ((newLen > 0) && (newBuf[newLen - 1] != '\n') && ((newLen + 1) < sizeof(newBuf))) {
        newBuf[newLen] = '\n';
        newLen++;
        newBuf[newLen] = '\0';
    }

    if ((newLen + 128) < sizeof(newBuf)) {
        newLen += wsprintfA(newBuf + newLen, "fullscreen=%d\n", fullscreen);
        newLen += wsprintfA(newBuf + newLen, "bpp=%d\n", bpp);
        newLen += wsprintfA(newBuf + newLen, "detail=%d\n", detail);
        newLen += wsprintfA(newBuf + newLen, "width=%d\n", width);
        newLen += wsprintfA(newBuf + newLen, "height=%d\n", height);
    }

    fp = fopen("resolution.ini", "wb");
    if (fp != NULL) {
        fwrite(newBuf, 1, newLen, fp);
        fclose(fp);
    }
}

/* 0-based index of whichever radio button in [first, last] is currently
   checked, or fallbackIndex if none is (shouldn't normally happen, since
   initial state always checks exactly one). */
static int GetCheckedInRange(HWND hwnd, int first, int last, int fallbackIndex)
{
    int id;
    for (id = first; id <= last; id++) {
        if (IsDlgButtonChecked(hwnd, id) == BST_CHECKED)
            return id - first;
    }
    return fallbackIndex;
}

/* Reads the current radio selections and (re)writes resolution.ini --
   called right before launching AA.exe (not AAServer.exe, which never
   reads this file). */
static void ApplyDisplaySettingsToIni(HWND hwnd)
{
    static const int bppValues[5]  = { 0, 8, 16, 24, 32 };
    /* Window size tier per detail level -- see WriteResolutionIni's own
       comment for why this is needed at all (an explicit width=/height=
       otherwise pins the window regardless of detail level). All 4:3,
       matching resolution.ini's own default aspect. Capped at the level-3
       size for levels 4-6 rather than continuing to grow the window:
       beyond that point higher detail is about render sharpness, not
       needing an even bigger window, and this stays within what
       RESSCALE's own resolution fallback ladder (1024x768/800x600/
       640x480) actually covers if the requested size isn't available. */
    static const int widthValues[6]  = { 640, 800, 1024, 1280, 1280, 1280 };
    static const int heightValues[6] = { 480, 600,  768,  960,  960,  960 };
    int fullscreen = GetCheckedInRange(hwnd, ID_RADIO_WINDOWED, ID_RADIO_FULLSCREEN, 1);
    int bppIdx     = GetCheckedInRange(hwnd, ID_RADIO_BPP_AUTO, ID_RADIO_BPP_32, 0);
    int detailIdx  = GetCheckedInRange(hwnd, ID_RADIO_DETAIL_1, ID_RADIO_DETAIL_6, 1);

    WriteResolutionIni(fullscreen, bppValues[bppIdx], detailIdx + 1,
        widthValues[detailIdx], heightValues[detailIdx]);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND: {
        char host[256], port[64];

        switch (LOWORD(wParam)) {
        case ID_BTN_SERVER:
            GetEditText(g_portEdit, port, sizeof(port));
            LaunchProcess("AAServer.exe", port, NULL);
            break;

        case ID_BTN_SINGLE:
            ApplyDisplaySettingsToIni(hwnd);
            LaunchProcess("AA.exe", NULL, NULL);
            PostQuitMessage(0);
            break;

        case ID_BTN_NETWORK:
            GetEditText(g_hostEdit, host, sizeof(host));
            if (host[0] == '\0') {
                MessageBoxA(hwnd, "Enter a server IP/host first.", "Play Network", MB_ICONWARNING | MB_OK);
                break;
            }
            GetEditText(g_portEdit, port, sizeof(port));
            ApplyDisplaySettingsToIni(hwnd);
            LaunchProcess("AA.exe", host, port);
            PostQuitMessage(0);
            break;

        case ID_RADIO_WINDOWED:
        case ID_RADIO_FULLSCREEN:
            CheckRadioButton(hwnd, ID_RADIO_WINDOWED, ID_RADIO_FULLSCREEN, LOWORD(wParam));
            break;

        case ID_RADIO_BPP_AUTO:
        case ID_RADIO_BPP_8:
        case ID_RADIO_BPP_16:
        case ID_RADIO_BPP_24:
        case ID_RADIO_BPP_32:
            CheckRadioButton(hwnd, ID_RADIO_BPP_AUTO, ID_RADIO_BPP_32, LOWORD(wParam));
            break;

        case ID_RADIO_DETAIL_1:
        case ID_RADIO_DETAIL_2:
        case ID_RADIO_DETAIL_3:
        case ID_RADIO_DETAIL_4:
        case ID_RADIO_DETAIL_5:
        case ID_RADIO_DETAIL_6:
            CheckRadioButton(hwnd, ID_RADIO_DETAIL_1, ID_RADIO_DETAIL_6, LOWORD(wParam));
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    int iniFullscreen, iniBpp, iniDetail, iniBppIdx;

    (void)hPrevInstance;
    (void)lpCmdLine;

    GetSelfDir();

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "AALauncherWin9x";
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "AALauncherWin9x", "Amulets & Armor Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 340, 540,
        NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) {
        return 1;
    }

    CreateWindowExA(0, "STATIC", "Server IP (for Play Network):",
        WS_CHILD | WS_VISIBLE, 10, 10, 300, 16, hwnd, NULL, hInstance, NULL);
    g_hostEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 28, 300, 22,
        hwnd, (HMENU)ID_EDIT_HOST, hInstance, NULL);

    CreateWindowExA(0, "STATIC", "Port (optional):",
        WS_CHILD | WS_VISIBLE, 10, 56, 300, 16, hwnd, NULL, hInstance, NULL);
    g_portEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 74, 300, 22,
        hwnd, (HMENU)ID_EDIT_PORT, hInstance, NULL);

    /* Display mode */
    CreateWindowExA(0, "BUTTON", "Display",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 105, 300, 48,
        hwnd, NULL, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "Windowed",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_RADIOBUTTON, 20, 123, 130, 20,
        hwnd, (HMENU)ID_RADIO_WINDOWED, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "Fullscreen",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 160, 123, 130, 20,
        hwnd, (HMENU)ID_RADIO_FULLSCREEN, hInstance, NULL);

    /* Color depth */
    CreateWindowExA(0, "BUTTON", "Color Depth",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 160, 300, 48,
        hwnd, NULL, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "Auto",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_RADIOBUTTON, 20, 178, 52, 20,
        hwnd, (HMENU)ID_RADIO_BPP_AUTO, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "8-bit",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 76, 178, 52, 20,
        hwnd, (HMENU)ID_RADIO_BPP_8, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "16-bit",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 132, 178, 56, 20,
        hwnd, (HMENU)ID_RADIO_BPP_16, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "24-bit",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 192, 178, 56, 20,
        hwnd, (HMENU)ID_RADIO_BPP_24, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "32-bit",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 252, 178, 56, 20,
        hwnd, (HMENU)ID_RADIO_BPP_32, hInstance, NULL);

    /* Level of detail -- also picks the window size, not just the
       internal 3D render density: see WriteResolutionIni's own comment
       for why the two are tied together. 1-6, matching HIGHRES_MAX_SCALE/
       VIEW3D_SCALE_MAX (see their own comments in Include/HIGHRES.H and
       Include/3D_VIEW.H). */
    CreateWindowExA(0, "BUTTON", "Level of Detail (also sets window size)",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 215, 300, 155,
        hwnd, NULL, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "1 (fastest, 640x480)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_RADIOBUTTON, 20, 233, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_1, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "2 (800x600)",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 20, 254, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_2, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "3 (1024x768)",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 20, 275, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_3, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "4 (1280x960)",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 20, 296, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_4, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "5 (1280x960)",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 20, 317, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_5, hInstance, NULL);
    CreateWindowExA(0, "BUTTON", "6 (sharpest, 1280x960)",
        WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, 20, 338, 280, 20,
        hwnd, (HMENU)ID_RADIO_DETAIL_6, hInstance, NULL);

    CreateWindowExA(0, "BUTTON", "Start Server",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 382, 300, 26,
        hwnd, (HMENU)ID_BTN_SERVER, hInstance, NULL);

    CreateWindowExA(0, "BUTTON", "Play Single Player",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 414, 300, 26,
        hwnd, (HMENU)ID_BTN_SINGLE, hInstance, NULL);

    CreateWindowExA(0, "BUTTON", "Play Network",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 446, 300, 26,
        hwnd, (HMENU)ID_BTN_NETWORK, hInstance, NULL);

    /* Pre-select radios from any existing resolution.ini (defaults match
       RESSCALE.C's own compiled-in defaults: fullscreen=1, bpp=0/auto,
       detail=2) so re-running the launcher reflects the last choice. */
    iniFullscreen = ReadIniInt("fullscreen", 1) ? 1 : 0;
    CheckRadioButton(hwnd, ID_RADIO_WINDOWED, ID_RADIO_FULLSCREEN,
        iniFullscreen ? ID_RADIO_FULLSCREEN : ID_RADIO_WINDOWED);

    iniBpp = ReadIniInt("bpp", 0);
    switch (iniBpp) {
        case 8:  iniBppIdx = ID_RADIO_BPP_8;  break;
        case 16: iniBppIdx = ID_RADIO_BPP_16; break;
        case 24: iniBppIdx = ID_RADIO_BPP_24; break;
        case 32: iniBppIdx = ID_RADIO_BPP_32; break;
        default: iniBppIdx = ID_RADIO_BPP_AUTO; break;
    }
    CheckRadioButton(hwnd, ID_RADIO_BPP_AUTO, ID_RADIO_BPP_32, iniBppIdx);

    iniDetail = ReadIniInt("detail", 2);
    if (iniDetail < 1) iniDetail = 1;
    if (iniDetail > 6) iniDetail = 6;
    CheckRadioButton(hwnd, ID_RADIO_DETAIL_1, ID_RADIO_DETAIL_6,
        ID_RADIO_DETAIL_1 + (iniDetail - 1));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
