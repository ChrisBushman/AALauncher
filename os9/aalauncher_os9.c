/* aalauncher_os9.c -- Amulets & Armor launcher for classic Mac OS 9 (CFM/PPC).
 *
 * A native Toolbox app (no Qt -- Qt doesn't exist on OS 9). It presents the
 * launcher UI and starts the OS 9 builds of the game, dedicated server, and
 * script compiler that live alongside it (AA / AAServer / AAScriptCompiler),
 * via the Process Manager.
 *
 * PHASE 1: window + button bar + menu + launching the sibling apps. The web
 * view (macsurf) and the Network/Display dialogs land in later phases; the
 * web-view area is a titled placeholder for now.
 *
 * Types come from the MacHeaders prefix force-included by AALauncher_OS9_Prefix.h.
 */
#include <Processes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ menus */
#define kAppleMenuID    128
#define kFileMenuID     129
#define kOptionsMenuID  130

#define kAboutItem      1        /* Apple menu */
#define kQuitItem       1        /* File menu */
#define kDisplayItem    1        /* Options menu */

/* ---------------------------------------------------------------- globals */
static Boolean    gDone      = false;
static WindowPtr  gWindow    = NULL;

static ControlHandle gBtnSingle  = NULL;
static ControlHandle gBtnNetwork = NULL;
static ControlHandle gBtnServer  = NULL;
static ControlHandle gBtnScript  = NULL;
static ControlHandle gBtnExit    = NULL;

/* --------------------------------------------------------- launch a sibling
 * Launch an application that sits in the same folder as this launcher.
 * (Classic Mac has no argv, so no arguments are passed here -- config
 *  hand-off to AA/AAServer is a later phase via a shared settings file.)
 */
static OSErr LaunchSibling(ConstStr255Param name)
{
    ProcessSerialNumber psn;
    ProcessInfoRec      info;
    FSSpec              appSpec;
    FSSpec              sibSpec;
    LaunchParamBlockRec lpb;
    OSErr               err;

    psn.highLongOfPSN = 0;
    psn.lowLongOfPSN  = kCurrentProcess;

    info.processInfoLength = sizeof(info);
    info.processName       = NULL;
    info.processAppSpec    = &appSpec;
    err = GetProcessInformation(&psn, &info);
    if (err != noErr)
        return err;

    /* sibling file in the launcher's own folder */
    err = FSMakeFSSpec(appSpec.vRefNum, appSpec.parID, name, &sibSpec);
    if (err != noErr)
        return err;

    lpb.launchBlockID      = extendedBlock;
    lpb.launchEPBLength    = extendedBlockLen;
    lpb.launchFileFlags    = 0;
    lpb.launchControlFlags = launchContinue | launchNoFileFlags;
    lpb.launchAppSpec      = &sibSpec;
    lpb.launchAppParameters = NULL;
    return LaunchApplication(&lpb);
}

static void ReportLaunchError(ConstStr255Param name, OSErr err)
{
    Str255 msg;
    Str255 numStr;

    /* "Could not launch <name> (error N)." via ParamText + a simple alert. */
    (void)err;
    BlockMoveData("\pCould not launch ", msg, 17);
    /* append name */
    BlockMoveData(name + 1, msg + 1 + msg[0], name[0]);
    msg[0] = (unsigned char)(17 + name[0]);
    NumToString((long)err, numStr);
    ParamText(msg, numStr, "\p", "\p");
    (void)StopAlert(128, NULL);
}

static void DoLaunch(ConstStr255Param name)
{
    OSErr err = LaunchSibling(name);
    if (err != noErr)
        ReportLaunchError(name, err);
}

/* ------------------------------------------------------------------ menus */
static void SetUpMenus(void)
{
    MenuHandle appleMenu, fileMenu, optionsMenu;

    appleMenu = NewMenu(kAppleMenuID, "\p\024");   /* 0x14 = Apple char */
    AppendMenu(appleMenu, "\pAbout Amulets & Armor Launcher;(-");
    AppendResMenu(appleMenu, 'DRVR');
    InsertMenu(appleMenu, 0);

    fileMenu = NewMenu(kFileMenuID, "\pFile");
    AppendMenu(fileMenu, "\pQuit/Q");
    InsertMenu(fileMenu, 0);

    optionsMenu = NewMenu(kOptionsMenuID, "\pOptions");
    AppendMenu(optionsMenu, "\pDisplay Settings\311");   /* \311 = ellipsis */
    InsertMenu(optionsMenu, 0);

    DrawMenuBar();
}

/* --------------------------------------------------------------- window UI */
static ControlHandle MakeButton(short left, short top, short width,
                                ConstStr255Param title)
{
    Rect r;
    SetRect(&r, left, top, (short)(left + width), (short)(top + 20));
    return NewControl(gWindow, &r, title, true, 0, 0, 1, pushButProc, 0);
}

static void MakeWindow(void)
{
    Rect bounds;

    SetRect(&bounds, 40, 60, 40 + 520, 60 + 360);
    gWindow = NewCWindow(NULL, &bounds, "\pAmulets & Armor Launcher",
                         true, documentProc, (WindowPtr)-1, true, 0);
    SetPort(gWindow);

    /* Button bar (two rows of three) across the bottom. */
    gBtnSingle  = MakeButton( 20, 250, 150, "\pPlay Single Player");
    gBtnNetwork = MakeButton(185, 250, 150, "\pPlay Network Game\311");
    gBtnServer  = MakeButton(350, 250, 150, "\pStart A&A Server");
    gBtnScript  = MakeButton( 20, 290, 150, "\pScript Compiler\311");
    gBtnExit    = MakeButton(350, 290, 150, "\pExit");
    /* (Display Settings lives in the Options menu, matching the Qt launcher.) */
}

static void DrawContent(void)
{
    Rect webRect;
    Str255 line1 = "\pAmulets & Armor";
    Str255 line2 = "\p(web view coming soon)";

    /* The area above the button bar is where the embedded web view will go.
       For now, frame it and label it. */
    SetRect(&webRect, 20, 20, 500, 235);
    FrameRect(&webRect);

    TextFont(0); TextFace(bold); TextSize(24);
    MoveTo(40, 120);
    DrawString(line1);
    TextFace(normal); TextSize(12);
    MoveTo(40, 145);
    DrawString(line2);
    TextSize(0);

    DrawControls(gWindow);
}

/* -------------------------------------------------- Display Settings dialog
 * Reads/writes resolution.ini in the launcher's own folder (= the game's
 * folder, since they ship together), which the game reads for windowed/
 * fullscreen, color depth, level-of-detail (also window size), and vsync.
 * Matches the Qt launcher's DisplaySettingsDialog (merge-preserving every
 * other line in the file).
 */
typedef struct {
    int fullscreen;   /* 1/0                */
    int bpp;          /* 0=auto, 8/16/24/32 */
    int detail;       /* 1..6               */
    int vsync;        /* 1/0                */
} ResSettings;

/* Read one line, breaking on \r, \n, or \r\n (OS 9 config files are CR- or
   LF-terminated); strips the terminator. Returns 0 at EOF. */
static int ReadLine(FILE *f, char *buf, int max)
{
    int c, n = 0;
    c = fgetc(f);
    if (c == EOF)
        return 0;
    while (c != EOF && c != '\r' && c != '\n') {
        if (n < max - 1)
            buf[n++] = (char)c;
        c = fgetc(f);
    }
    if (c == '\r') {                 /* swallow a following \n of a \r\n pair */
        int c2 = fgetc(f);
        if (c2 != '\n' && c2 != EOF)
            ungetc(c2, f);
    }
    buf[n] = '\0';
    return 1;
}

static int StartsWith(const char *s, const char *p)
{
    return strncmp(s, p, strlen(p)) == 0;
}

static int IsManagedKey(const char *line)
{
    return StartsWith(line, "fullscreen=") || StartsWith(line, "bpp=") ||
           StartsWith(line, "detail=")     || StartsWith(line, "width=") ||
           StartsWith(line, "height=")     || StartsWith(line, "vsync=");
}

static void ReadRes(ResSettings *s)
{
    FILE *f;
    char line[256];
    char *eq;
    /* defaults match RESSCALE.C's own compiled-in defaults */
    s->fullscreen = 1; s->bpp = 0; s->detail = 2; s->vsync = 0;
    f = fopen("resolution.ini", "rb");
    if (f == NULL)
        return;
    while (ReadLine(f, line, sizeof(line))) {
        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        {
            int v = atoi(eq + 1);
            if      (strcmp(line, "fullscreen") == 0) s->fullscreen = v;
            else if (strcmp(line, "bpp") == 0)        s->bpp = v;
            else if (strcmp(line, "detail") == 0)     s->detail = v;
            else if (strcmp(line, "vsync") == 0)      s->vsync = v;
        }
    }
    fclose(f);
}

static void WriteRes(const ResSettings *s)
{
    /* window size tier per detail level, capped at level-3 (all 4:3) */
    static const int W[6] = { 640, 1024, 1280, 1280, 1280, 1280 };
    static const int H[6] = { 480,  768,  960,  960,  960,  960 };
    char *kept[128];
    int nkept = 0, i;
    int det = s->detail;
    FILE *in, *out;
    char line[256];

    if (det < 1) det = 1;
    if (det > 6) det = 6;

    in = fopen("resolution.ini", "rb");
    if (in != NULL) {
        while (ReadLine(in, line, sizeof(line))) {
            if (IsManagedKey(line))
                continue;
            if (nkept < 128) {
                kept[nkept] = (char *)malloc(strlen(line) + 1);
                if (kept[nkept] != NULL) {
                    strcpy(kept[nkept], line);
                    nkept++;
                }
            }
        }
        fclose(in);
    }

    out = fopen("resolution.ini", "wb");
    if (out == NULL) {
        for (i = 0; i < nkept; i++) free(kept[i]);
        return;
    }
    for (i = 0; i < nkept; i++) {
        fprintf(out, "%s\n", kept[i]);
        free(kept[i]);
    }
    fprintf(out, "fullscreen=%d\n", s->fullscreen);
    fprintf(out, "bpp=%d\n",        s->bpp);
    fprintf(out, "detail=%d\n",     det);
    fprintf(out, "width=%d\n",      W[det - 1]);
    fprintf(out, "height=%d\n",     H[det - 1]);
    fprintf(out, "vsync=%d\n",      s->vsync);
    fclose(out);
}

static short GetDItemValue(DialogPtr d, short item)
{
    short type; Handle h; Rect box;
    GetDialogItem(d, item, &type, &h, &box);
    return GetControlValue((ControlHandle)h);
}

static void SetDItemValue(DialogPtr d, short item, short val)
{
    short type; Handle h; Rect box;
    GetDialogItem(d, item, &type, &h, &box);
    SetControlValue((ControlHandle)h, val);
}

static void SetRadioGroup(DialogPtr d, short first, short last, short selected)
{
    short i;
    for (i = first; i <= last; i++)
        SetDItemValue(d, i, (short)((i == selected) ? 1 : 0));
}

/* DITL 129 item numbers */
#define kDsOK        1
#define kDsCancel    2
#define kDsWindowed  4
#define kDsFullscr   5
#define kDsBppAuto   7
#define kDsBpp8      8
#define kDsBpp16     9
#define kDsBpp24     10
#define kDsBpp32     11
#define kDsDetail1   13   /* ..18 for detail 1..6 (item = 12 + detail) */
#define kDsDetail6   18
#define kDsVsync     19

static void DoDisplaySettings(void)
{
    DialogPtr d;
    ResSettings s;
    short item, bppItem, detailItem;
    Boolean done = false;

    ReadRes(&s);

    d = GetNewDialog(129, NULL, (WindowPtr)-1);
    if (d == NULL) {
        SysBeep(1);
        return;
    }
    SetDialogDefaultItem(d, kDsOK);

    SetRadioGroup(d, kDsWindowed, kDsFullscr, s.fullscreen ? kDsFullscr : kDsWindowed);
    bppItem = kDsBppAuto;
    if      (s.bpp == 8)  bppItem = kDsBpp8;
    else if (s.bpp == 16) bppItem = kDsBpp16;
    else if (s.bpp == 24) bppItem = kDsBpp24;
    else if (s.bpp == 32) bppItem = kDsBpp32;
    SetRadioGroup(d, kDsBppAuto, kDsBpp32, bppItem);
    detailItem = (short)(12 + (s.detail < 1 ? 1 : (s.detail > 6 ? 6 : s.detail)));
    SetRadioGroup(d, kDsDetail1, kDsDetail6, detailItem);
    SetDItemValue(d, kDsVsync, (short)(s.vsync ? 1 : 0));

    ShowWindow(GetDialogWindow(d));

    while (!done) {
        ModalDialog(NULL, &item);
        if (item == kDsOK) {
            short i;
            s.fullscreen = GetDItemValue(d, kDsFullscr) ? 1 : 0;
            if      (GetDItemValue(d, kDsBpp8))  s.bpp = 8;
            else if (GetDItemValue(d, kDsBpp16)) s.bpp = 16;
            else if (GetDItemValue(d, kDsBpp24)) s.bpp = 24;
            else if (GetDItemValue(d, kDsBpp32)) s.bpp = 32;
            else                                 s.bpp = 0;
            s.detail = 2;
            for (i = kDsDetail1; i <= kDsDetail6; i++)
                if (GetDItemValue(d, i)) { s.detail = i - 12; break; }
            s.vsync = GetDItemValue(d, kDsVsync) ? 1 : 0;
            WriteRes(&s);
            done = true;
        } else if (item == kDsCancel) {
            done = true;
        } else if (item >= kDsWindowed && item <= kDsFullscr) {
            SetRadioGroup(d, kDsWindowed, kDsFullscr, item);
        } else if (item >= kDsBppAuto && item <= kDsBpp32) {
            SetRadioGroup(d, kDsBppAuto, kDsBpp32, item);
        } else if (item >= kDsDetail1 && item <= kDsDetail6) {
            SetRadioGroup(d, kDsDetail1, kDsDetail6, item);
        } else if (item == kDsVsync) {
            SetDItemValue(d, kDsVsync, (short)(GetDItemValue(d, kDsVsync) ? 0 : 1));
        }
    }
    DisposeDialog(d);
}

/* ----------------------------------------------------- Network Game dialog
 * Writes aanet.ini in the launcher's folder (= the game's folder) with the
 * server address, then launches AA -- which reads it (classic Mac has no
 * argv). Single player deletes aanet.ini so no stale address forces it online.
 */
static void WriteAANet(ConstStr255Param host, ConstStr255Param port)
{
    FILE *f;
    char h[256], p[256];
    short i;
    for (i = 0; i < host[0]; i++) h[i] = (char)host[i + 1];
    h[host[0]] = '\0';
    for (i = 0; i < port[0]; i++) p[i] = (char)port[i + 1];
    p[port[0]] = '\0';
    f = fopen("aanet.ini", "wb");
    if (f == NULL)
        return;
    fprintf(f, "server=%s\n", h);
    if (p[0] != '\0')
        fprintf(f, "port=%s\n", p);
    fclose(f);
}

static void DeleteAANet(void)
{
    remove("aanet.ini");
}

/* DITL 130 items */
#define kNwConnect  1
#define kNwCancel   2
#define kNwHost     4
#define kNwPort     6

static void DoNetworkGame(void)
{
    DialogPtr d;
    short item, type;
    Handle h;
    Rect box;
    Str255 host, port;

    d = GetNewDialog(130, NULL, (WindowPtr)-1);
    if (d == NULL) {
        SysBeep(1);
        return;
    }
    SetDialogDefaultItem(d, kNwConnect);
    GetDialogItem(d, kNwPort, &type, &h, &box);
    SetDialogItemText(h, "\p21300");
    SelectDialogItemText(d, kNwHost, 0, 0);
    ShowWindow(GetDialogWindow(d));

    for (;;) {
        ModalDialog(NULL, &item);
        if (item == kNwConnect) {
            GetDialogItem(d, kNwHost, &type, &h, &box);
            GetDialogItemText(h, host);
            GetDialogItem(d, kNwPort, &type, &h, &box);
            GetDialogItemText(h, port);
            if (host[0] == 0) {          /* need a host/IP */
                SysBeep(1);
                continue;
            }
            WriteAANet(host, port);
            DisposeDialog(d);
            DoLaunch("\pAA");            /* game reads aanet.ini and joins */
            gDone = true;               /* close the launcher, like the Qt version */
            return;
        } else if (item == kNwCancel) {
            DisposeDialog(d);
            return;
        }
    }
}

/* ------------------------------------------------------------- event loop */
static void DoMenuChoice(long choice)
{
    short menuID = (short)(choice >> 16);
    short item   = (short)(choice & 0xFFFF);
    Str255 name;

    switch (menuID) {
        case kAppleMenuID:
            if (item == kAboutItem) {
                ParamText("\pAmulets & Armor Launcher for Mac OS 9",
                          "\p", "\p", "\p");
                (void)NoteAlert(128, NULL);
            } else {
                GetMenuItemText(GetMenuHandle(kAppleMenuID), item, name);
                (void)OpenDeskAcc(name);
            }
            break;
        case kFileMenuID:
            if (item == kQuitItem)
                gDone = true;
            break;
        case kOptionsMenuID:
            if (item == kDisplayItem)
                DoDisplaySettings();
            break;
    }
    HiliteMenu(0);
}

static void DoButton(ControlHandle ctl)
{
    if (ctl == gBtnSingle) {
        DeleteAANet();               /* single player: no stale server address */
        DoLaunch("\pAA");
        gDone = true;                /* close the launcher after launching, like Qt */
    }
    else if (ctl == gBtnServer)  DoLaunch("\pAAServer");
    else if (ctl == gBtnScript)  DoLaunch("\pAAScriptCompiler");
    else if (ctl == gBtnNetwork) DoNetworkGame();
    else if (ctl == gBtnExit)    gDone = true;
}

static void DoMouseDown(EventRecord *ev)
{
    WindowPtr    whichWindow;
    short        part;
    ControlHandle ctl;
    Point        pt;

    part = FindWindow(ev->where, &whichWindow);
    switch (part) {
        case inMenuBar:
            DoMenuChoice(MenuSelect(ev->where));
            break;
        case inContent:
            if (whichWindow != FrontWindow()) {
                SelectWindow(whichWindow);
            } else {
                SetPort(gWindow);
                pt = ev->where;
                GlobalToLocal(&pt);
                part = FindControl(pt, gWindow, &ctl);
                if (part && ctl != NULL) {
                    if (TrackControl(ctl, pt, NULL))
                        DoButton(ctl);
                }
            }
            break;
        case inDrag:
            DragWindow(whichWindow, ev->where, &qd.screenBits.bounds);
            break;
        case inGoAway:
            if (TrackGoAway(whichWindow, ev->where))
                gDone = true;
            break;
    }
}

static void DoUpdate(EventRecord *ev)
{
    WindowPtr w = (WindowPtr)ev->message;
    BeginUpdate(w);
    if (w == gWindow) {
        SetPort(gWindow);
        DrawContent();
    }
    EndUpdate(w);
}

static void EventLoop(void)
{
    EventRecord ev;

    while (!gDone) {
        if (WaitNextEvent(everyEvent, &ev, 10, NULL)) {
            switch (ev.what) {
                case mouseDown:
                    DoMouseDown(&ev);
                    break;
                case keyDown:
                case autoKey:
                    if ((ev.modifiers & cmdKey) != 0) {
                        char ch = (char)(ev.message & charCodeMask);
                        DoMenuChoice(MenuKey(ch));
                    }
                    break;
                case updateEvt:
                    DoUpdate(&ev);
                    break;
            }
        }
    }
}

/* ------------------------------------------------------------------- main */
static void InitToolbox(void)
{
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
}

int main(void)
{
    InitToolbox();
    SetUpMenus();
    MakeWindow();
    EventLoop();
    return 0;
}
