/* Amulets & Armor IRIX Launcher -- plain Xt/Xaw native UI, with an
 * embedded NetSurf (nsfb) web view showing the game's website. Since
 * nsfb is a standalone SDL 1.2 application, not a linkable widget, the
 * "embedding" works by creating a plain Xt Core widget (which has a
 * real X11 window once realized), then spawning nsfb as a detached
 * subprocess with the SDL_WINDOWID environment variable set to that
 * window's ID -- SDL 1.2's X11 driver checks for this and reparents
 * its own window into the given one instead of creating a new
 * top-level window. See project memory (project_netsurf_irix_port /
 * project_irix_webview_deferred) for the history here.
 * Behavior otherwise matches the Windows/Qt/Cocoa launchers: buttons
 * that spawn AA/AAServer/AAScriptCompiler as detached subprocesses (or,
 * for the script compiler, a captured-output child -- see
 * ScriptCompilerCallback), an editable server port field, and a "Play
 * Network Game" dialog listing known servers (saved favorites plus
 * live LAN discoveries -- see StartDiscovery) alongside manual host/
 * port entry. Xt + Xaw only -- both are part of any IRIX X11R6 install,
 * no Nekoware/TGCware dependency for the launcher itself (nsfb, spawned
 * separately, does depend on Nekoware's SDL/libpng/etc). */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Toggle.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "title_image.h"

/* Protocol matches AAServer's DiscoveryServerLoop (AAServer.cpp) and
   ipxserver.h's DEFAULT_DISCOVERY_PORT/DISCOVERY_REQUEST_MAGIC/
   DISCOVERY_REPLY_PREFIX -- kept as literals here rather than a shared
   header since the launcher and server are separate repos/build systems. */
#define DISCOVERY_PORT 21399
#define DISCOVERY_REQUEST_MAGIC "AASERVER_DISCOVER"
#define DISCOVERY_REPLY_PREFIX  "AASERVER_HERE:"

/* The site's own CSS (#canvas/#container) is a fixed 1024px-wide
 * layout -- size the embedded view a bit wider than that so netsurf's
 * own vertical scrollbar (needed since the page is much taller than
 * any reasonable window) doesn't itself force horizontal scrolling on
 * top of the vertical scrolling a user is expected to do. Height is
 * just "a reasonable amount of the O2's 1600x1024 desktop", not the
 * page's full height -- vertical scrolling within the view is normal
 * and expected, matching how a user would use a real browser window. */
#define WEBVIEW_WIDTH  1044
#define WEBVIEW_HEIGHT 700
#define WEBVIEW_URL    "http://www.amuletsandarmor.com/index.htm"

static XtAppContext G_appContext;
static Widget G_topLevel;
static Visual *G_visual = NULL;
static int G_depth = 0;
static char *G_selfDir;
static char *G_aaBinary;
static char *G_serverBinary;
static char *G_nsfbBinary;
static char *G_nsfbResDir;
static char *G_port;
static char *G_scriptCompilerBinary;

/* Persistent (non-popup) port entry in the main window -- a Dialog
   widget works fine as a plain managed child, not just inside a popup
   shell, and gives us a label+editable-value pair for free (see the old
   IP-entry dialog below for the same widget class used as a popup). */
static Widget G_portDialog;

/* Known-servers list: saved favorites (servers.txt alongside the
   launcher binary) plus live LAN-discovered entries, shown together in
   the Join dialog's List widget. */
#define MAX_SERVER_ENTRIES 64
static char *G_serverLabels[MAX_SERVER_ENTRIES];
static char *G_serverHosts[MAX_SERVER_ENTRIES];
static char *G_serverPorts[MAX_SERVER_ENTRIES];
static int G_serverCount = 0;

/*--------------------------------------------------------------------------*/
static void ComputeSelfDir(const char *argv0)
{
    char cwd[MAXPATHLEN];
    char resolved[MAXPATHLEN * 2];
    char *slash;

    if (argv0[0] == '/')  {
        strncpy(resolved, argv0, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
    } else {
        /* Relative path (with or without a directory component) or a
           bare name -- either way, assume it's being run from its own
           directory (the common case for a shipped, not-installed
           binary: "./AALauncher" or a full path from a file manager). */
        getcwd(cwd, sizeof(cwd));
        sprintf(resolved, "%s/%s", cwd, argv0);
    }

    slash = strrchr(resolved, '/');
    if (slash != NULL)  {
        int len = (int)(slash - resolved);
        G_selfDir = malloc(len + 1);
        memcpy(G_selfDir, resolved, len);
        G_selfDir[len] = '\0';
    } else {
        G_selfDir = strdup(".");
    }
}

static char *DefaultBinary(const char *name)
{
    char *result = malloc(strlen(G_selfDir) + strlen(name) + 2);
    sprintf(result, "%s/%s", G_selfDir, name);
    return result;
}

/*--------------------------------------------------------------------------*/
/* Known-servers list: servers.txt (name\thost\tport per line, '#'
   comments/blank lines ignored) alongside the launcher binary, plus any
   live LAN-discovered entries added at runtime via AddServerEntry. */
static Widget G_joinList;  /* set once the Join dialog is built; NULL until then */

static void RebuildServerListWidget(void)
{
    if (G_joinList == NULL)
        return;
    XawListChange(G_joinList, G_serverLabels, G_serverCount, 0, True);
}

static void AddServerEntry(const char *label, const char *host, const char *port)
{
    if (G_serverCount >= MAX_SERVER_ENTRIES)
        return;
    G_serverLabels[G_serverCount] = strdup(label);
    G_serverHosts[G_serverCount] = strdup(host);
    G_serverPorts[G_serverCount] = strdup(port);
    G_serverCount++;
    RebuildServerListWidget();
}

/* Unlike the other 3 platforms (which build a fresh dialog object each
   time "Play Network Game" is clicked), the Join dialog here is a single
   popup shell reused across opens -- so without this, reopening it would
   re-run discovery and re-append the same LAN servers on top of what's
   already in G_server* from the previous time it was open. Called at the
   start of every PlayNetworkCallback, not just the first. */
static void ResetServerList(void)
{
    int i;
    for (i = 0; i < G_serverCount; i++)  {
        free(G_serverLabels[i]);
        free(G_serverHosts[i]);
        free(G_serverPorts[i]);
    }
    G_serverCount = 0;
    RebuildServerListWidget();
}

static char *ServersFilePath(void)
{
    return DefaultBinary("servers.txt");
}

static void LoadSavedServers(void)
{
    char *path = ServersFilePath();
    FILE *fp = fopen(path, "r");
    char line[512];

    free(path);
    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL)  {
        char *nl = strchr(line, '\n');
        char *name, *host, *port;
        if (nl != NULL)
            *nl = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        name = line;
        host = strchr(name, '\t');
        if (host == NULL)
            continue;
        *host++ = '\0';
        port = strchr(host, '\t');
        if (port == NULL)
            continue;
        *port++ = '\0';

        {
            char label[300];
            sprintf(label, "%.100s  (%.100s:%.32s)", name, host, port);
            AddServerEntry(label, host, port);
        }
    }
    fclose(fp);
}

static void AppendSavedServer(const char *name, const char *host, const char *port)
{
    char *path = ServersFilePath();
    FILE *fp = fopen(path, "a");
    char label[300];

    free(path);
    if (fp == NULL)
        return;
    fprintf(fp, "%s\t%s\t%s\n", name, host, port);
    fclose(fp);

    sprintf(label, "%.100s  (%.100s:%.32s)", name, host, port);
    AddServerEntry(label, host, port);
}

/*--------------------------------------------------------------------------*/
/* LAN discovery: broadcast a probe, then poll for replies (via
   XtAppAddInput on the socket) until a timeout (via XtAppAddTimeOut)
   closes it. Raw BSD sockets, integrated into the Xt event loop rather
   than blocking it -- same protocol as AAServer's DiscoveryServerLoop. */
static int G_discoverySocket = -1;
static XtInputId G_discoveryInputId;
static XtIntervalId G_discoveryTimeoutId;

static void DiscoveryStop(void)
{
    if (G_discoverySocket >= 0)  {
        XtRemoveInput(G_discoveryInputId);
        close(G_discoverySocket);
        G_discoverySocket = -1;
    }
}

static void DiscoveryTimeout(XtPointer clientData, XtIntervalId *id)
{
    DiscoveryStop();
}

static void DiscoveryReadable(XtPointer clientData, int *fd, XtInputId *id)
{
    char buffer[128];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    ssize_t n = recvfrom(*fd, buffer, sizeof(buffer) - 1, 0,
                          (struct sockaddr *)&fromAddr, &fromLen);
    const char *prefix = DISCOVERY_REPLY_PREFIX;
    size_t prefixLen = strlen(prefix);
    char *rest, *colon;
    char label[300];

    if (n <= 0)
        return;
    buffer[n] = '\0';

    if (strncmp(buffer, prefix, prefixLen) != 0)
        return;
    rest = buffer + prefixLen;
    colon = strchr(rest, ':');
    if (colon == NULL)
        return;
    *colon = '\0';

    /* rest = port, colon+1 = name */
    sprintf(label, "%.100s [LAN]  (%.100s:%.32s)", colon + 1, inet_ntoa(fromAddr.sin_addr), rest);
    AddServerEntry(label, inet_ntoa(fromAddr.sin_addr), rest);
}

static void StartDiscovery(void)
{
    int sock;
    int broadcastEnable = 1;
    struct sockaddr_in destAddr;
    const char *request = DISCOVERY_REQUEST_MAGIC;

    DiscoveryStop();

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return;

    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(DISCOVERY_PORT);
    destAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(sock, request, strlen(request), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));

    G_discoverySocket = sock;
    G_discoveryInputId = XtAppAddInput(G_appContext, sock, (XtPointer)XtInputReadMask,
                                        DiscoveryReadable, NULL);
    G_discoveryTimeoutId = XtAppAddTimeOut(G_appContext, 1500, DiscoveryTimeout, NULL);
}

/*--------------------------------------------------------------------------*/
/* Minimal modal message popup (used for launch errors). */
static void ShowMessage(const char *title, const char *message)
{
    Widget shell, dialog;
    Arg args[4];
    int n;

    n = 0;
    XtSetArg(args[n], XtNtitle, title); n++;
    shell = XtCreatePopupShell("messageShell", transientShellWidgetClass,
                                G_topLevel, args, n);

    n = 0;
    XtSetArg(args[n], XtNlabel, message); n++;
    dialog = XtCreateManagedWidget("messageDialog", dialogWidgetClass,
                                    shell, args, n);
    XawDialogAddButton(dialog, "OK", (XtCallbackProc)XtPopdown, shell);

    XtPopup(shell, XtGrabExclusive);
}

/*--------------------------------------------------------------------------*/
#define MAX_EXTRA_ENV 4

/* extraEnv, if non-NULL, is a NULL-terminated array of already-formatted
   "NAME=value" strings (e.g. SDL_WINDOWID, NETSURFRES) set in the child
   before exec -- see LaunchWebView. */
static void LaunchProcessEnv(const char *binaryPath, char *const argv[],
                              const char *const extraEnv[])
{
    pid_t pid = fork();
    if (pid == 0)  {
        /* Child: detach into its own session so it survives the
           launcher exiting, then exec. */
        /* Static: putenv() keeps a pointer to this string, not a copy, and
           must stay valid through execv(). No setenv() on this IRIX libc. */
        static char n32Buf[2048];
        static char plainBuf[2048];
        static char extraBuf[MAX_EXTRA_ENV][256];
        char libPath[1024];
        char *existingN32 = getenv("LD_LIBRARYN32_PATH");
        char *existing = getenv("LD_LIBRARY_PATH");
        int i;

        setsid();
        chdir(G_selfDir);

        /* AA/AAServer/nsfb link against bundled SDL libs shipped in a
           lib/ folder alongside each binary (see their own run.sh
           wrappers); N32 binaries only find them via LD_LIBRARYN32_PATH
           (and plain LD_LIBRARY_PATH as a fallback) since we're exec'ing
           them directly rather than through a shell wrapper. */
        sprintf(libPath, "%s/lib", G_selfDir);
        if (existingN32 && existingN32[0])
            sprintf(n32Buf, "LD_LIBRARYN32_PATH=%s:%s:/usr/nekoware/lib:/usr/tgcware/lib", libPath, existingN32);
        else
            sprintf(n32Buf, "LD_LIBRARYN32_PATH=%s:/usr/nekoware/lib:/usr/tgcware/lib", libPath);
        putenv(n32Buf);

        if (existing && existing[0])
            sprintf(plainBuf, "LD_LIBRARY_PATH=%s:%s:/usr/nekoware/lib:/usr/tgcware/lib", libPath, existing);
        else
            sprintf(plainBuf, "LD_LIBRARY_PATH=%s:/usr/nekoware/lib:/usr/tgcware/lib", libPath);
        putenv(plainBuf);

        for (i = 0; extraEnv != NULL && extraEnv[i] != NULL && i < MAX_EXTRA_ENV; i++)  {
            strncpy(extraBuf[i], extraEnv[i], sizeof(extraBuf[i]) - 1);
            extraBuf[i][sizeof(extraBuf[i]) - 1] = '\0';
            putenv(extraBuf[i]);
        }

        execv(binaryPath, argv);
        /* execv only returns on failure. */
        _exit(127);
    } else if (pid < 0)  {
        ShowMessage("Launch Error", "Failed to fork a new process.");
    }
}

static void LaunchProcess(const char *binaryPath, char *const argv[])
{
    LaunchProcessEnv(binaryPath, argv, NULL);
}

static void StartServerCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    char *portValue = XawDialogGetValueString(G_portDialog);
    char *argv[3];
    int i = 0;

    argv[i++] = G_serverBinary;
    if (portValue && portValue[0])
        argv[i++] = portValue;
    argv[i] = NULL;

    LaunchProcess(G_serverBinary, argv);
}

static void PlaySinglePlayerCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    char *argv[2];
    argv[0] = G_aaBinary;
    argv[1] = NULL;
    LaunchProcess(G_aaBinary, argv);
    exit(0);
}

static void ExitCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    exit(0);
}

/*--------------------------------------------------------------------------*/
/* "Play Network Game" dialog: a list of known servers (saved favorites +
   live LAN discoveries), plus a manual host/port/name entry. Selecting a
   list row just populates the host/port fields (Xaw's List widget has no
   built-in double-click notion to distinguish "select" from "activate")
   -- Connect is still a separate, explicit step either way. */
static Widget G_joinShell, G_joinHostDialog, G_joinPortDialog, G_joinNameDialog;

static void JoinDialogCancel(Widget w, XtPointer clientData, XtPointer callData)
{
    DiscoveryStop();
    XtPopdown(G_joinShell);
}

static void JoinDialogConnect(Widget w, XtPointer clientData, XtPointer callData)
{
    char *host = XawDialogGetValueString(G_joinHostDialog);
    char *port = XawDialogGetValueString(G_joinPortDialog);
    char *argv[4];
    int i = 0;

    DiscoveryStop();
    XtPopdown(G_joinShell);

    if (host == NULL || host[0] == '\0')
        return;

    argv[i++] = G_aaBinary;
    argv[i++] = host;
    if (port && port[0])
        argv[i++] = port;
    argv[i] = NULL;

    LaunchProcess(G_aaBinary, argv);
    exit(0);
}

static void JoinDialogSave(Widget w, XtPointer clientData, XtPointer callData)
{
    char *host = XawDialogGetValueString(G_joinHostDialog);
    char *port = XawDialogGetValueString(G_joinPortDialog);
    char *name = XawDialogGetValueString(G_joinNameDialog);

    if (host == NULL || host[0] == '\0' || port == NULL || port[0] == '\0')
        return;
    if (name == NULL || name[0] == '\0')
        name = host;

    AppendSavedServer(name, host, port);
}

/* Xaw's Dialog widget has XawDialogGetValueString() but no setter --
   reach its internal "value" child (an AsciiText widget; this is how
   every other Xaw program that needs to do this does it too) and set
   its string resource directly. */
static void SetDialogValue(Widget dialog, const char *value)
{
    Widget valueWidget = XtNameToWidget(dialog, "value");
    Arg args[1];

    if (valueWidget == NULL)
        return;
    XtSetArg(args[0], XtNstring, value);
    XtSetValues(valueWidget, args, 1);
}

static void JoinListCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    XawListReturnStruct *ret = (XawListReturnStruct *)callData;
    if (ret == NULL || ret->list_index == XAW_LIST_NONE)
        return;
    if (ret->list_index >= G_serverCount)
        return;

    SetDialogValue(G_joinHostDialog, G_serverHosts[ret->list_index]);
    SetDialogValue(G_joinPortDialog, G_serverPorts[ret->list_index]);
}

static void PlayNetworkCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    Arg args[6];
    int n;

    if (G_joinShell == NULL)  {
        Widget listLabel, listViewport;
        char *defaultPort = XawDialogGetValueString(G_portDialog);

        n = 0;
        XtSetArg(args[n], XtNtitle, "AALauncher Network Connection"); n++;
        G_joinShell = XtCreatePopupShell("joinShell", transientShellWidgetClass,
                                          G_topLevel, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Known servers:"); n++;
        XtSetArg(args[n], XtNborderWidth, 0); n++;
        listLabel = XtCreateManagedWidget("listLabel", labelWidgetClass,
                                           G_joinShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNfromVert, listLabel); n++;
        XtSetArg(args[n], XtNwidth, 340); n++;
        XtSetArg(args[n], XtNheight, 120); n++;
        XtSetArg(args[n], XtNallowVert, True); n++;
        listViewport = XtCreateManagedWidget("listViewport", viewportWidgetClass,
                                              G_joinShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNlist, G_serverLabels); n++;
        XtSetArg(args[n], XtNnumberStrings, G_serverCount); n++;
        XtSetArg(args[n], XtNdefaultColumns, 1); n++;
        XtSetArg(args[n], XtNforceColumns, True); n++;
        G_joinList = XtCreateManagedWidget("joinList", listWidgetClass,
                                            listViewport, args, n);
        XtAddCallback(G_joinList, XtNcallback, JoinListCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Host/IP:"); n++;
        XtSetArg(args[n], XtNvalue, ""); n++;
        XtSetArg(args[n], XtNfromVert, listViewport); n++;
        G_joinHostDialog = XtCreateManagedWidget("joinHostDialog", dialogWidgetClass,
                                                  G_joinShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Port:"); n++;
        XtSetArg(args[n], XtNvalue, defaultPort ? defaultPort : "21300"); n++;
        XtSetArg(args[n], XtNfromVert, G_joinHostDialog); n++;
        G_joinPortDialog = XtCreateManagedWidget("joinPortDialog", dialogWidgetClass,
                                                  G_joinShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Name (to save):"); n++;
        XtSetArg(args[n], XtNvalue, ""); n++;
        XtSetArg(args[n], XtNfromVert, G_joinPortDialog); n++;
        G_joinNameDialog = XtCreateManagedWidget("joinNameDialog", dialogWidgetClass,
                                                  G_joinShell, args, n);
        XawDialogAddButton(G_joinNameDialog, "Save", JoinDialogSave, NULL);
        XawDialogAddButton(G_joinNameDialog, "Connect", JoinDialogConnect, NULL);
        XawDialogAddButton(G_joinNameDialog, "Cancel", JoinDialogCancel, NULL);
    }

    ResetServerList();
    LoadSavedServers();
    StartDiscovery();
    XtPopup(G_joinShell, XtGrabExclusive);
}

/*--------------------------------------------------------------------------*/
/* "Display Settings" dialog -- Fullscreen/Windowed, Color Depth, and Level
   of Detail, writing directly to resolution.ini (read by AA.exe's RESSCALE
   module, see AmuletsArmor's Source/Win32/RESSCALE.C) in the same
   directory as G_aaBinary. Same 3 settings and same resolution.ini format
   as the other launchers (see mac/DisplaySettingsDialog.cpp,
   ppc/DisplaySettingsController.m, win9x/main.c). Radio-button grouping
   uses Xaw's Toggle widget XtNradioGroup/XtNradioData mechanism: each
   group's first Toggle anchors XtNradioGroup for the rest, XtNradioData
   identifies which one is selected, and XawToggleSetCurrent/
   XawToggleGetCurrent (passing any widget in the group) select/read it. */
static Widget G_displayShell = NULL;
static Widget G_dispWindowedToggle, G_dispFullscreenToggle;
static Widget G_dispBppToggle[5];     /* radioData: 0(auto),8,16,24,32 */
static Widget G_dispDetailToggle[6];  /* radioData: 1..6 */

static char *DirOfPath(const char *path)
{
    char *result = strdup(path);
    char *slash = strrchr(result, '/');
    if (slash != NULL)  {
        *slash = '\0';
    } else {
        free(result);
        result = strdup(".");
    }
    return result;
}

static char *DisplaySettingsIniPath(void)
{
    char *dir = DirOfPath(G_aaBinary);
    char *result = malloc(strlen(dir) + strlen("/resolution.ini") + 1);
    sprintf(result, "%s/resolution.ini", dir);
    free(dir);
    return result;
}

static void LoadDisplaySettingsFromIni(void)
{
    /* Defaults match RESSCALE.C's own compiled-in defaults (fullscreen=1,
       bpp=0/auto, detail=2) so this dialog shows the same choice AA.exe
       itself would default to before resolution.ini exists. */
    int fullscreen = 1, bpp = 0, detail = 2;
    char *path = DisplaySettingsIniPath();
    FILE *fp = fopen(path, "r");
    char line[256];

    free(path);
    if (fp != NULL)  {
        while (fgets(line, sizeof(line), fp) != NULL)  {
            char key[64];
            int value;
            if (sscanf(line, "%63[^=]=%d", key, &value) == 2)  {
                if (strcmp(key, "fullscreen") == 0) fullscreen = value;
                else if (strcmp(key, "bpp") == 0) bpp = value;
                else if (strcmp(key, "detail") == 0) detail = value;
            }
        }
        fclose(fp);
    }

    if (detail < 1) detail = 1;
    if (detail > 6) detail = 6;

    XawToggleSetCurrent(G_dispWindowedToggle, (XtPointer)(long)fullscreen);
    XawToggleSetCurrent(G_dispBppToggle[0], (XtPointer)(long)bpp);
    XawToggleSetCurrent(G_dispDetailToggle[0], (XtPointer)(long)detail);
}

static void SaveDisplaySettingsToIni(void)
{
    /* Rewrites resolution.ini with the current selections, preserving
       every other line (comments, scale=/fit=/aspect=/hotkeys=) exactly
       as found -- same merge-preserve approach as the other launchers'
       ini writers, for the same reason: an explicit width=/height=
       otherwise pins the window regardless of detail level (RESSCALE.C's
       IComputeWindowSize gives explicit width=/height= priority over
       scale=), so window size needs to be rewritten alongside detail
       level, not left stale. */
    int fullscreen = (int)(long)XawToggleGetCurrent(G_dispWindowedToggle);
    int bpp = (int)(long)XawToggleGetCurrent(G_dispBppToggle[0]);
    int detail = (int)(long)XawToggleGetCurrent(G_dispDetailToggle[0]);
    static const int widths[6]  = { 640, 1024, 1280, 1280, 1280, 1280 };
    static const int heights[6] = { 480,  768,  960,  960,  960,  960 };
    int width, height;
    char *path, *tmpPath;
    FILE *inFp, *outFp;
    char line[256];

    if (detail < 1) detail = 1;
    if (detail > 6) detail = 6;
    width = widths[detail - 1];
    height = heights[detail - 1];

    path = DisplaySettingsIniPath();
    tmpPath = malloc(strlen(path) + 5);
    sprintf(tmpPath, "%s.tmp", path);

    outFp = fopen(tmpPath, "w");
    if (outFp == NULL)  {
        free(path);
        free(tmpPath);
        return;
    }

    inFp = fopen(path, "r");
    if (inFp != NULL)  {
        while (fgets(line, sizeof(line), inFp) != NULL)  {
            if (strncmp(line, "fullscreen=", 11) == 0) continue;
            if (strncmp(line, "bpp=", 4) == 0) continue;
            if (strncmp(line, "detail=", 7) == 0) continue;
            if (strncmp(line, "width=", 6) == 0) continue;
            if (strncmp(line, "height=", 7) == 0) continue;
            fputs(line, outFp);
        }
        fclose(inFp);
    }

    fprintf(outFp, "fullscreen=%d\n", fullscreen);
    fprintf(outFp, "bpp=%d\n", bpp);
    fprintf(outFp, "detail=%d\n", detail);
    fprintf(outFp, "width=%d\n", width);
    fprintf(outFp, "height=%d\n", height);
    fclose(outFp);

    rename(tmpPath, path);
    free(tmpPath);
    free(path);
}

static void DisplaySettingsOk(Widget w, XtPointer clientData, XtPointer callData)
{
    SaveDisplaySettingsToIni();
    XtPopdown(G_displayShell);
}

static void DisplaySettingsCancel(Widget w, XtPointer clientData, XtPointer callData)
{
    XtPopdown(G_displayShell);
}

static void DisplaySettingsCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    Arg args[8];
    int n;

    if (G_displayShell == NULL)  {
        Widget form2, displayLabel, bppLabel, detailLabel, okButton, cancelButton;
        Widget prevWidget;
        int i;
        static const char *bppLabels[5] = { "Auto (recommended)", "8-bit", "16-bit", "24-bit", "32-bit" };
        static const int bppValues[5] = { 0, 8, 16, 24, 32 };
        static const char *detailLabels[6] = {
            "1 (fastest, 640x480)", "2 (1024x768)", "3 (1280x960)",
            "4 (1280x960)", "5 (1280x960)", "6 (sharpest, 1280x960)",
        };

        n = 0;
        XtSetArg(args[n], XtNtitle, "Display Settings"); n++;
        G_displayShell = XtCreatePopupShell("displayShell", transientShellWidgetClass,
                                             G_topLevel, args, n);

        n = 0;
        form2 = XtCreateManagedWidget("displayForm", formWidgetClass, G_displayShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Display:"); n++;
        XtSetArg(args[n], XtNborderWidth, 0); n++;
        displayLabel = XtCreateManagedWidget("displayLabel", labelWidgetClass, form2, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Windowed"); n++;
        XtSetArg(args[n], XtNfromVert, displayLabel); n++;
        XtSetArg(args[n], XtNradioData, (XtPointer)(long)0); n++;
        G_dispWindowedToggle = XtCreateManagedWidget("dispWindowed", toggleWidgetClass, form2, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Fullscreen"); n++;
        XtSetArg(args[n], XtNfromVert, displayLabel); n++;
        XtSetArg(args[n], XtNfromHoriz, G_dispWindowedToggle); n++;
        XtSetArg(args[n], XtNradioGroup, G_dispWindowedToggle); n++;
        XtSetArg(args[n], XtNradioData, (XtPointer)(long)1); n++;
        G_dispFullscreenToggle = XtCreateManagedWidget("dispFullscreen", toggleWidgetClass, form2, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Color Depth:"); n++;
        XtSetArg(args[n], XtNborderWidth, 0); n++;
        XtSetArg(args[n], XtNfromVert, G_dispWindowedToggle); n++;
        bppLabel = XtCreateManagedWidget("bppLabel", labelWidgetClass, form2, args, n);

        prevWidget = NULL;
        for (i = 0; i < 5; i++)  {
            n = 0;
            XtSetArg(args[n], XtNlabel, bppLabels[i]); n++;
            XtSetArg(args[n], XtNfromVert, bppLabel); n++;
            if (prevWidget != NULL)  {
                XtSetArg(args[n], XtNfromHoriz, prevWidget); n++;
                XtSetArg(args[n], XtNradioGroup, G_dispBppToggle[0]); n++;
            }
            XtSetArg(args[n], XtNradioData, (XtPointer)(long)bppValues[i]); n++;
            G_dispBppToggle[i] = XtCreateManagedWidget("dispBpp", toggleWidgetClass, form2, args, n);
            prevWidget = G_dispBppToggle[i];
        }

        n = 0;
        XtSetArg(args[n], XtNlabel, "Level of Detail (also sets window size):"); n++;
        XtSetArg(args[n], XtNborderWidth, 0); n++;
        XtSetArg(args[n], XtNfromVert, G_dispBppToggle[0]); n++;
        detailLabel = XtCreateManagedWidget("detailLabel", labelWidgetClass, form2, args, n);

        prevWidget = detailLabel;
        for (i = 0; i < 6; i++)  {
            n = 0;
            XtSetArg(args[n], XtNlabel, detailLabels[i]); n++;
            XtSetArg(args[n], XtNfromVert, prevWidget); n++;
            XtSetArg(args[n], XtNleft, XtChainLeft); n++;
            if (i > 0)  {
                XtSetArg(args[n], XtNradioGroup, G_dispDetailToggle[0]); n++;
            }
            XtSetArg(args[n], XtNradioData, (XtPointer)(long)(i + 1)); n++;
            G_dispDetailToggle[i] = XtCreateManagedWidget("dispDetail", toggleWidgetClass, form2, args, n);
            prevWidget = G_dispDetailToggle[i];
        }

        n = 0;
        XtSetArg(args[n], XtNlabel, "Cancel"); n++;
        XtSetArg(args[n], XtNfromVert, prevWidget); n++;
        cancelButton = XtCreateManagedWidget("dispCancel", commandWidgetClass, form2, args, n);
        XtAddCallback(cancelButton, XtNcallback, DisplaySettingsCancel, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "OK"); n++;
        XtSetArg(args[n], XtNfromVert, prevWidget); n++;
        XtSetArg(args[n], XtNfromHoriz, cancelButton); n++;
        okButton = XtCreateManagedWidget("dispOk", commandWidgetClass, form2, args, n);
        XtAddCallback(okButton, XtNcallback, DisplaySettingsOk, NULL);
    }

    LoadDisplaySettingsFromIni();
    XtPopup(G_displayShell, XtGrabExclusive);
}

/*--------------------------------------------------------------------------*/
/* Web view area, above the button row. Normally shows the embedded
   nsfb (NetSurf) browser via SDL_WINDOWID reparenting -- see
   LaunchWebView. If the nsfb binary can't be found (e.g. a partial
   install), falls back to a static piece of the game's own title art
   instead, embedded as raw RGB (title_image.h) so that fallback has no
   image-library dependency at all. */
static Widget G_webviewArea;
static XImage *G_titleXImage = NULL;
static Boolean G_webviewSpawned = False;

/* Scale an 8-bit colour component down to the width of a visual's colour
   mask and shift it into position -- works for any TrueColor/DirectColor
   depth (15/16/24/32-bit) without hardcoding a specific one. */
static unsigned long ComponentToMask(unsigned char component, unsigned long mask)
{
    int shift = 0;
    int bits = 0;
    unsigned long m = mask;

    if (m == 0)
        return 0;
    while ((m & 1) == 0)  {
        m >>= 1;
        shift++;
    }
    while (m & 1)  {
        bits++;
        m >>= 1;
    }
    return ((unsigned long)(component >> (8 - bits))) << shift;
}

/* Explicit RGB -> pixel using G_visual's own masks, for widget background/
   foreground colors -- Xaw's automatic named-color resolution (e.g. the
   "gray"/"white" background default) comes out visibly wrong (a blue
   tint) against the freshly-created StaticColor colormap used for the
   whole app (see main()); computing pixels the same way as the title
   image, which does render correctly, sidesteps whatever's misbehaving
   in that lookup rather than chasing it further. */
static unsigned long ComputePixel(unsigned char r, unsigned char g, unsigned char b)
{
    return ComponentToMask(r, G_visual->red_mask) |
           ComponentToMask(g, G_visual->green_mask) |
           ComponentToMask(b, G_visual->blue_mask);
}

static void BuildTitleXImage(Widget w)
{
    Display *dpy = XtDisplay(w);
    Visual *visual = G_visual;
    int depth = G_depth;
    const unsigned char *p;
    int x, y;

    /* G_visual/G_depth are set once in main() from the same XVisualInfo
       used to create the shell (StaticColor if available, this
       display's default visual otherwise) -- NOT re-queried from the
       widget via XtGetValues(XtNvisual), which unexpectedly comes back
       NULL on this IRIX Xt/Xaw build despite XtNdepth on the same call
       working fine; not worth chasing further when the value is already
       known from setup. */
    if (G_titleXImage != NULL)
        return;

    G_titleXImage = XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL,
                                  TITLE_IMAGE_WIDTH, TITLE_IMAGE_HEIGHT, 8, 0);
    G_titleXImage->data =
        malloc((size_t)G_titleXImage->bytes_per_line * TITLE_IMAGE_HEIGHT);

    p = G_titleImageRGB;
    for (y = 0; y < TITLE_IMAGE_HEIGHT; y++)  {
        for (x = 0; x < TITLE_IMAGE_WIDTH; x++)  {
            unsigned char r = *p++;
            unsigned char g = *p++;
            unsigned char b = *p++;
            unsigned long pixel = ComponentToMask(r, visual->red_mask) |
                                   ComponentToMask(g, visual->green_mask) |
                                   ComponentToMask(b, visual->blue_mask);
            XPutPixel(G_titleXImage, x, y, pixel);
        }
    }
}

/* Only used as a fallback when nsfb couldn't be launched into this area
   (see LaunchWebView) -- draws the static title image centered within
   the (much larger, sized for the real web view) widget area. */
static void WebviewAreaExpose(Widget w, XtPointer clientData, XEvent *event, Boolean *cont)
{
    Display *dpy;
    Window win;
    GC gc;
    int destX, destY;

    if (G_webviewSpawned)
        return;

    if (event->type != Expose || event->xexpose.count != 0)
        return;

    BuildTitleXImage(w);

    dpy = XtDisplay(w);
    win = XtWindow(w);
    gc = XCreateGC(dpy, win, 0, NULL);

    destX = (WEBVIEW_WIDTH - TITLE_IMAGE_WIDTH) / 2;
    destY = (WEBVIEW_HEIGHT - TITLE_IMAGE_HEIGHT) / 2;
    if (destX < 0) destX = 0;
    if (destY < 0) destY = 0;

    XClearArea(dpy, win, 0, 0, WEBVIEW_WIDTH, WEBVIEW_HEIGHT, False);
    XPutImage(dpy, win, gc, G_titleXImage, 0, 0, destX, destY,
              TITLE_IMAGE_WIDTH, TITLE_IMAGE_HEIGHT);
    XFreeGC(dpy, gc);
}

/* Spawn nsfb (NetSurf's framebuffer frontend) reparented into winId via
   SDL_WINDOWID -- see the file-level comment for how this works. */
static void LaunchWebView(Window winId)
{
    char *argv[9];
    char widthArg[16], heightArg[16];
    char windowIdEnv[64];
    char resPathEnv[1024];
    const char *extraEnv[3];

    /* Embedded in the launcher, nsfb's own browser chrome (nav buttons,
       URL bar, "Done (N.Ns)" status text) doesn't make sense to show --
       the launcher provides its own navigation. "q" is create_toolbar()'s
       own documented sentinel for "disable the bar entirely" (zero
       height reserved, not just an empty bar) -- an actually-empty
       value doesn't work, because nsoption's string parser deliberately
       normalises empty strings back to NULL ("do not allow empty
       strings in text options"), which just falls through to the
       default layout again. toolbar_status_size=0 collapses the status
       text area's width to 0 (its horizontal scrollbar neighbour still
       shows, which is fine/expected). Both of those are ordinary
       nsoption "--name=value" overrides, parsed by nsoption_commandline
       and stripped from argv before nsfb's own getopt-based
       process_cmdline ever sees them -- which is also why they must
       come *before* the "-w"/"-h" flags below: nsoption_commandline
       stops at the first non "--"-prefixed argument.

       -w/-h are needed because nsfb does not infer its size from the
       SDL_WINDOWID window it's embedding into -- without them it just
       uses its own compiled-in 800x600 default regardless of the
       actual window's size, leaving most of WEBVIEW_WIDTH x
       WEBVIEW_HEIGHT blank (confirmed on real hardware). */
    sprintf(widthArg, "%d", WEBVIEW_WIDTH);
    sprintf(heightArg, "%d", WEBVIEW_HEIGHT);

    argv[0] = G_nsfbBinary;
    argv[1] = "--fb_toolbar_layout=q";
    argv[2] = "--toolbar_status_size=0";
    argv[3] = "-w";
    argv[4] = widthArg;
    argv[5] = "-h";
    argv[6] = heightArg;
    argv[7] = WEBVIEW_URL;
    argv[8] = NULL;

    sprintf(windowIdEnv, "SDL_WINDOWID=%lu", (unsigned long)winId);

    /* nsfb's own resource search path (NETSURF_FB_RESPATH, baked in at
       build time) only finds its CSS/Messages/icon resources via either
       an absolute install-prefix path or a path relative to its cwd --
       LaunchProcessEnv chdir()s to the *launcher's* directory before
       exec'ing, same as it does for AA/AAServer, which breaks the
       relative fallback. NETSURFRES is nsfb's own documented override
       for exactly this case. */
    sprintf(resPathEnv, "NETSURFRES=%s/frontends/framebuffer/res", G_nsfbResDir);

    extraEnv[0] = windowIdEnv;
    extraEnv[1] = resPathEnv;
    extraEnv[2] = NULL;

    LaunchProcessEnv(G_nsfbBinary, argv, extraEnv);
    G_webviewSpawned = True;
    /* Mouse/keyboard input reaching the embedded view is a separate,
       still-open problem (see project memory) -- an XSetInputFocus
       attempt here hit its own BadMatch (likely a race: this runs right
       after fork(), before nsfb has started/mapped anything) and was
       removed rather than adding an untested workaround on top of an
       already-uncertain fix. */
}

/*--------------------------------------------------------------------------*/
/* Script Compiler window: a standalone top-level (not a popup transient
   to the main window, unlike the Join dialog -- this one stays open and
   usable alongside the main launcher window, matching the other 3
   platforms' non-modal script compiler window). Wraps AAScriptCompiler's
   "SC <script> <output>" CLI: text fields for the two paths (no native
   file chooser in plain Xaw, so typed paths only), a Compile button, and
   a read-only AsciiText log fed from the child's stdout/stderr via a
   pipe, read asynchronously through XtAppAddInput so it doesn't block
   the launcher's event loop. */
static Widget G_scShell = NULL;
static Widget G_scScriptDialog, G_scOutputDialog, G_scCompileButton, G_scLogText;
static pid_t G_scChildPid = -1;
static XtInputId G_scInputId;
static char *G_scLogBuffer = NULL;
static size_t G_scLogLen = 0, G_scLogCap = 0;

static void ScAppendLog(const char *data, size_t len)
{
    Arg args[1];

    if (G_scLogLen + len + 1 > G_scLogCap)  {
        size_t newCap = (G_scLogCap == 0) ? 4096 : G_scLogCap * 2;
        while (newCap < G_scLogLen + len + 1)
            newCap *= 2;
        G_scLogBuffer = realloc(G_scLogBuffer, newCap);
        G_scLogCap = newCap;
    }
    memcpy(G_scLogBuffer + G_scLogLen, data, len);
    G_scLogLen += len;
    G_scLogBuffer[G_scLogLen] = '\0';

    XtSetArg(args[0], XtNstring, G_scLogBuffer);
    XtSetValues(G_scLogText, args, 1);
}

static void ScReadable(XtPointer clientData, int *fd, XtInputId *id)
{
    char buf[512];
    ssize_t n = read(*fd, buf, sizeof(buf));

    if (n <= 0)  {
        int status = 0;
        const char *doneMsg;

        XtRemoveInput(G_scInputId);
        close(*fd);
        if (G_scChildPid > 0)  {
            waitpid(G_scChildPid, &status, 0);
            G_scChildPid = -1;
        }
        doneMsg = (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            ? "\n-- Compile succeeded. --\n" : "\n-- Compile failed. --\n";
        ScAppendLog(doneMsg, strlen(doneMsg));
        XtSetSensitive(G_scCompileButton, True);
        return;
    }
    ScAppendLog(buf, (size_t)n);
}

static void ScCompileCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    char *scriptPath = XawDialogGetValueString(G_scScriptDialog);
    char *outputPath = XawDialogGetValueString(G_scOutputDialog);
    int pipefd[2];
    pid_t pid;
    Arg args[1];

    if (scriptPath == NULL || scriptPath[0] == '\0' || outputPath == NULL || outputPath[0] == '\0')  {
        ShowMessage("Script Compiler", "Please enter both a script file and an output file.");
        return;
    }
    if (access(G_scriptCompilerBinary, X_OK) != 0)  {
        ShowMessage("Script Compiler", "Compiler binary not found.");
        return;
    }

    G_scLogLen = 0;
    if (G_scLogBuffer != NULL)
        G_scLogBuffer[0] = '\0';
    XtSetArg(args[0], XtNstring, "");
    XtSetValues(G_scLogText, args, 1);
    XtSetSensitive(G_scCompileButton, False);

    if (pipe(pipefd) != 0)  {
        XtSetSensitive(G_scCompileButton, True);
        return;
    }

    pid = fork();
    if (pid == 0)  {
        char *cargv[4];
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        cargv[0] = G_scriptCompilerBinary;
        cargv[1] = scriptPath;
        cargv[2] = outputPath;
        cargv[3] = NULL;
        execv(G_scriptCompilerBinary, cargv);
        _exit(127);
    } else if (pid > 0)  {
        close(pipefd[1]);
        G_scChildPid = pid;
        G_scInputId = XtAppAddInput(G_appContext, pipefd[0], (XtPointer)XtInputReadMask,
                                     ScReadable, NULL);
    } else {
        close(pipefd[0]);
        close(pipefd[1]);
        XtSetSensitive(G_scCompileButton, True);
    }
}

static void ScriptCompilerCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    Arg args[10];
    int n;

    if (G_scShell == NULL)  {
        Widget scForm, logViewport;

        n = 0;
        XtSetArg(args[n], XtNtitle, "Amulets & Armor Script Compiler"); n++;
        XtSetArg(args[n], XtNwidth, 600); n++;
        XtSetArg(args[n], XtNheight, 420); n++;
        G_scShell = XtAppCreateShell("AALauncherScriptCompiler", "AALauncherScriptCompiler",
                                      topLevelShellWidgetClass, XtDisplay(G_topLevel), args, n);

        n = 0;
        scForm = XtCreateManagedWidget("scForm", formWidgetClass, G_scShell, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Script file:"); n++;
        XtSetArg(args[n], XtNvalue, ""); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        G_scScriptDialog = XtCreateManagedWidget("scScriptDialog", dialogWidgetClass, scForm, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Output file:"); n++;
        XtSetArg(args[n], XtNvalue, ""); n++;
        XtSetArg(args[n], XtNfromVert, G_scScriptDialog); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        G_scOutputDialog = XtCreateManagedWidget("scOutputDialog", dialogWidgetClass, scForm, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Compile"); n++;
        XtSetArg(args[n], XtNfromVert, G_scOutputDialog); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        G_scCompileButton = XtCreateManagedWidget("scCompileButton", commandWidgetClass, scForm, args, n);
        XtAddCallback(G_scCompileButton, XtNcallback, ScCompileCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNfromVert, G_scCompileButton); n++;
        XtSetArg(args[n], XtNwidth, 570); n++;
        XtSetArg(args[n], XtNheight, 260); n++;
        XtSetArg(args[n], XtNallowVert, True); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbottom, XtChainBottom); n++;
        XtSetArg(args[n], XtNright, XtChainRight); n++;
        logViewport = XtCreateManagedWidget("scLogViewport", viewportWidgetClass, scForm, args, n);

        n = 0;
        XtSetArg(args[n], XtNstring, "Compiler output will appear here..."); n++;
        XtSetArg(args[n], XtNeditType, XawtextRead); n++;
        XtSetArg(args[n], XtNwidth, 560); n++;
        XtSetArg(args[n], XtNheight, 400); n++;
        G_scLogText = XtCreateManagedWidget("scLogText", asciiTextWidgetClass, logViewport, args, n);

        XtRealizeWidget(G_scShell);
    } else {
        XtMapWidget(G_scShell);
    }
}

/*--------------------------------------------------------------------------*/
static char *ArgValue(int argc, char **argv, const char *flag)
{
    int i;
    for (i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0)
            return argv[i + 1];
    return NULL;
}

int main(int argc, char **argv)
{
    Widget form, btnServer, btnNetwork, btnSingle, btnDisplaySettings, btnScriptCompiler, btnExit;
    Arg args[10];
    int n;
    char *aaPath, *serverPath, *webviewPath, *webviewResDir, *scriptCompilerPath;
    Display *dpy;
    XVisualInfo vinfo;

    /* This display's default visual is 8-bit PseudoColor (confirmed via
       xdpyinfo on the O2) -- indexed color with no real RGB masks, which
       is why a straightforward RGB-mask pixel-packing approach (see
       ComponentToMask/BuildTitleXImage) renders solid black there: every
       mask is 0. Prefer a real 24-bit TrueColor visual for the whole app
       instead (confirmed available via xdpyinfo) -- besides giving
       correct RGB masks throughout, this is required for the embedded
       web view (see LaunchWebView): SDL_WINDOWID embedding makes nsfb
       inherit whatever visual/depth the given window already has, and
       its 32bpp plotters need a real TrueColor surface or you get
       exactly the kind of sliced/striped pixel corruption you'd expect
       from writing 32-bit pixels into an 8-bit one (confirmed
       empirically). An earlier version of this file used an 8-bit
       StaticColor visual for everything except a separately-constructed
       TrueColor child window just for the web view; going all-TrueColor
       instead is simpler and avoids a class of X11-protocol-level
       depth-mismatch gotchas that approach ran into (a child window
       with a different depth than its parent needs an explicit
       colormap and border_pixel that Xt's own widget creation doesn't
       reliably supply). Falls back to 8-bit StaticColor, then the
       display default, if no TrueColor visual is available at all (the
       web view will likely render wrong in that case, but the rest of
       the UI still works). */
    XtToolkitInitialize();
    G_appContext = XtCreateApplicationContext();
    dpy = XtOpenDisplay(G_appContext, NULL, "AALauncher", "AALauncher", NULL, 0, &argc, argv);
    if (dpy == NULL)  {
        fprintf(stderr, "AALauncher: cannot open display\n");
        exit(1);
    }

    if (XMatchVisualInfo(dpy, DefaultScreen(dpy), 24, TrueColor, &vinfo) ||
        XMatchVisualInfo(dpy, DefaultScreen(dpy), 8, StaticColor, &vinfo))  {
        Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                         vinfo.visual, AllocNone);
        n = 0;
        XtSetArg(args[n], XtNvisual, vinfo.visual); n++;
        XtSetArg(args[n], XtNdepth, vinfo.depth); n++;
        XtSetArg(args[n], XtNcolormap, cmap); n++;
        G_topLevel = XtAppCreateShell("AALauncher", "AALauncher",
                                       applicationShellWidgetClass, dpy, args, n);
        G_visual = vinfo.visual;
        G_depth = vinfo.depth;
    } else {
        G_topLevel = XtAppCreateShell("AALauncher", "AALauncher",
                                       applicationShellWidgetClass, dpy, NULL, 0);
        G_visual = DefaultVisual(dpy, DefaultScreen(dpy));
        G_depth = DefaultDepth(dpy, DefaultScreen(dpy));
    }

    ComputeSelfDir(argv[0]);

    aaPath = ArgValue(argc, argv, "--aa-path");
    serverPath = ArgValue(argc, argv, "--server-path");
    scriptCompilerPath = ArgValue(argc, argv, "--script-compiler-path");
    webviewPath = ArgValue(argc, argv, "--webview-path");
    webviewResDir = ArgValue(argc, argv, "--webview-res-dir");
    G_port = ArgValue(argc, argv, "--port");

    G_aaBinary = aaPath ? strdup(aaPath) : DefaultBinary("AA");
    G_serverBinary = serverPath ? strdup(serverPath) : DefaultBinary("AAServer");
    G_scriptCompilerBinary = scriptCompilerPath ? strdup(scriptCompilerPath) : DefaultBinary("AAScriptCompiler");
    G_nsfbBinary = webviewPath ? strdup(webviewPath) : DefaultBinary("nsfb");
    /* See LaunchWebView -- this needs to contain a "frontends/framebuffer/
       res" subdirectory. Defaults to alongside the launcher itself, which
       is where a real release would ship it; overridden for dev testing
       against the netsurf-all source tree directly. */
    G_nsfbResDir = webviewResDir ? strdup(webviewResDir) : strdup(G_selfDir);

    n = 0;
    XtSetArg(args[n], XtNtitle, "Amulets & Armor IRIX Launcher v1.00"); n++;
    XtSetValues(G_topLevel, args, n);

    {
        /* See ComputePixel's comment -- Xaw's own default background
           color resolution renders visibly wrong (blue-tinted) against
           this app's freshly-created StaticColor colormap, so every
           widget below gets an explicit background/foreground pixel
           computed the same way the title image's pixels are. */
        Pixel bgPixel = (Pixel)ComputePixel(212, 208, 200);
        Pixel fgPixel = (Pixel)ComputePixel(0, 0, 0);

        n = 0;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        form = XtCreateManagedWidget("form", formWidgetClass, G_topLevel, args, n);

        /* Web view first (left-anchored provisionally -- corrected below)
           with the 3 buttons below it via fromVert. Uses its own
           TrueColor visual/depth/colormap (see main()), not the rest of
           the app's 8-bit StaticColor one -- Xt supports this per-widget. */
        n = 0;
        XtSetArg(args[n], XtNwidth, WEBVIEW_WIDTH); n++;
        XtSetArg(args[n], XtNheight, WEBVIEW_HEIGHT); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        G_webviewArea = XtCreateManagedWidget("webviewArea", widgetClass, form, args, n);
        XtAddEventHandler(G_webviewArea, ExposureMask, False, WebviewAreaExpose, NULL);

        /* Large, bold button font -- roughly 3x a typical default core-font
           size -- plus generous internal padding and inter-button spacing,
           per explicit feedback that the original buttons (sized off the
           default font) were too small. XLFD wildcards keep this portable
           across whatever fonts this X server actually has; if none match,
           leave XtNfont unset so Xt falls back to the widget's default. */
        XFontStruct *buttonFontStruct =
            XLoadQueryFont(dpy, "-*-*-bold-r-*-*-34-*-*-*-*-*-*-*");
        Dimension btnSpacing = 30;

        n = 0;
        XtSetArg(args[n], XtNlabel, "Start A&A Server"); n++;
        XtSetArg(args[n], XtNfromVert, G_webviewArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnServer = XtCreateManagedWidget("btnServer", commandWidgetClass, form, args, n);
        XtAddCallback(btnServer, XtNcallback, StartServerCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Play Network Game"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnServer); n++;
        XtSetArg(args[n], XtNhorizDistance, btnSpacing); n++;
        XtSetArg(args[n], XtNfromVert, G_webviewArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnNetwork = XtCreateManagedWidget("btnNetwork", commandWidgetClass, form, args, n);
        XtAddCallback(btnNetwork, XtNcallback, PlayNetworkCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Play Single Player"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnNetwork); n++;
        XtSetArg(args[n], XtNhorizDistance, btnSpacing); n++;
        XtSetArg(args[n], XtNfromVert, G_webviewArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnSingle = XtCreateManagedWidget("btnSingle", commandWidgetClass, form, args, n);
        XtAddCallback(btnSingle, XtNcallback, PlaySinglePlayerCallback, NULL);

        /* Second row, below the first: persistent server-port field (used
           both for "Start A&A Server" and as the Join dialog's default),
           plus Script Compiler / Exit buttons. Chained fromVert=btnServer
           (not G_webviewArea) to sit below the first row; horizDistance
           is corrected to match the first row's centering below, once
           realized. */
        n = 0;
        XtSetArg(args[n], XtNlabel, "Server Port:"); n++;
        XtSetArg(args[n], XtNvalue, (G_port && G_port[0]) ? G_port : "21300"); n++;
        XtSetArg(args[n], XtNfromVert, btnServer); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        G_portDialog = XtCreateManagedWidget("portDialog", dialogWidgetClass, form, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Display Settings"); n++;
        XtSetArg(args[n], XtNfromHoriz, G_portDialog); n++;
        XtSetArg(args[n], XtNhorizDistance, btnSpacing); n++;
        XtSetArg(args[n], XtNfromVert, btnServer); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnDisplaySettings = XtCreateManagedWidget("btnDisplaySettings", commandWidgetClass, form, args, n);
        XtAddCallback(btnDisplaySettings, XtNcallback, DisplaySettingsCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Script Compiler"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnDisplaySettings); n++;
        XtSetArg(args[n], XtNhorizDistance, btnSpacing); n++;
        XtSetArg(args[n], XtNfromVert, btnServer); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnScriptCompiler = XtCreateManagedWidget("btnScriptCompiler", commandWidgetClass, form, args, n);
        XtAddCallback(btnScriptCompiler, XtNcallback, ScriptCompilerCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Exit"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnScriptCompiler); n++;
        XtSetArg(args[n], XtNhorizDistance, btnSpacing); n++;
        XtSetArg(args[n], XtNfromVert, btnServer); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        XtSetArg(args[n], XtNinternalWidth, 20); n++;
        XtSetArg(args[n], XtNinternalHeight, 12); n++;
        if (buttonFontStruct != NULL)  {
            XtSetArg(args[n], XtNfont, buttonFontStruct); n++;
        }
        btnExit = XtCreateManagedWidget("btnExit", commandWidgetClass, form, args, n);
        XtAddCallback(btnExit, XtNcallback, ExitCallback, NULL);
    }

    /* Form widgets don't resolve constraint-based child positions until
       they're actually realized (see the equivalent comment this replaced,
       previously about centering the old narrow title-image placeholder
       over the button row -- now inverted, since the button row is
       narrower than the web view area and needs centering under it
       instead). Realizing first is also necessary before
       XtWindow(G_webviewArea) is valid, which LaunchWebView needs to hand
       nsfb a real window ID via SDL_WINDOWID. */
    XtRealizeWidget(G_topLevel);

    {
        Position serverX = 0, singleX = 0;
        Dimension singleWidth = 0;
        Dimension rowWidth;
        Dimension maxWidth;
        Position rowOffset, webviewOffset;

        n = 0;
        XtSetArg(args[n], XtNx, &serverX); n++;
        XtGetValues(btnServer, args, n);

        n = 0;
        XtSetArg(args[n], XtNx, &singleX); n++;
        XtSetArg(args[n], XtNwidth, &singleWidth); n++;
        XtGetValues(btnSingle, args, n);

        /* Whichever of the button row or the web view is wider (the 3x
           button-size feedback made the row wider than WEBVIEW_WIDTH in
           practice) defines the overall content width; centre the
           narrower of the two under it, rather than assuming the web
           view is always the wider element. */
        rowWidth = (Dimension)(singleX + (Position)singleWidth - serverX);
        maxWidth = (rowWidth > WEBVIEW_WIDTH) ? rowWidth : WEBVIEW_WIDTH;

        rowOffset = (maxWidth - rowWidth) / 2;
        if (rowOffset < 0)
            rowOffset = 0;
        n = 0;
        XtSetArg(args[n], XtNhorizDistance, rowOffset); n++;
        XtSetValues(btnServer, args, n);

        /* Second row's leading widget (the port field) needs the same
           left-edge offset as the first row's leading widget (btnServer)
           to keep both rows aligned under each other. */
        n = 0;
        XtSetArg(args[n], XtNhorizDistance, rowOffset); n++;
        XtSetValues(G_portDialog, args, n);

        webviewOffset = (maxWidth - WEBVIEW_WIDTH) / 2;
        if (webviewOffset < 0)
            webviewOffset = 0;
        n = 0;
        XtSetArg(args[n], XtNhorizDistance, webviewOffset); n++;
        XtSetValues(G_webviewArea, args, n);
    }

    if (access(G_nsfbBinary, X_OK) == 0)  {
        LaunchWebView(XtWindow(G_webviewArea));
    }
    /* else: WebviewAreaExpose's fallback (the static title image) handles
       showing something reasonable in the area instead. */

    XtAppMainLoop(G_appContext);

    return 0;
}
