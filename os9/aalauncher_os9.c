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
                SysBeep(1);   /* Display Settings dialog -- later phase */
            break;
    }
    HiliteMenu(0);
}

static void DoButton(ControlHandle ctl)
{
    if (ctl == gBtnSingle)       DoLaunch("\pAmuletsAndArmor");
    else if (ctl == gBtnServer)  DoLaunch("\pAAServer");
    else if (ctl == gBtnScript)  DoLaunch("\pAAScriptCompiler");
    else if (ctl == gBtnNetwork) SysBeep(1);   /* Network dialog -- later phase */
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
