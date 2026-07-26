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
 * Behavior otherwise matches the Windows/Qt/Cocoa launchers: 3 buttons
 * that spawn AA/AAServer as detached subprocesses, plus a small modal
 * dialog to enter a server IP for "Play Network Game". Xt + Xaw only
 * -- both are part of any IRIX X11R6 install, no Nekoware/TGCware
 * dependency for the launcher itself (nsfb, spawned separately, does
 * depend on Nekoware's SDL/libpng/etc). */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Dialog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include "title_image.h"

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
    char *argv[3];
    int i = 0;

    argv[i++] = G_serverBinary;
    if (G_port && G_port[0])
        argv[i++] = G_port;
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

/*--------------------------------------------------------------------------*/
/* "Play Network Game" IP-entry dialog. */
static Widget G_ipShell, G_ipDialog;

static void NetworkDialogCancel(Widget w, XtPointer clientData, XtPointer callData)
{
    XtPopdown(G_ipShell);
}

static void NetworkDialogConnect(Widget w, XtPointer clientData, XtPointer callData)
{
    char *ip = XawDialogGetValueString(G_ipDialog);
    char *argv[4];
    int i = 0;

    XtPopdown(G_ipShell);

    if (ip == NULL || ip[0] == '\0')
        return;

    argv[i++] = G_aaBinary;
    argv[i++] = ip;
    if (G_port && G_port[0])
        argv[i++] = G_port;
    argv[i] = NULL;

    LaunchProcess(G_aaBinary, argv);
    exit(0);
}

static void PlayNetworkCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    Arg args[4];
    int n;

    if (G_ipShell == NULL)  {
        n = 0;
        XtSetArg(args[n], XtNtitle, "AALauncher Network Connection"); n++;
        G_ipShell = XtCreatePopupShell("networkShell", transientShellWidgetClass,
                                        G_topLevel, args, n);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Enter Network IP of Server:"); n++;
        XtSetArg(args[n], XtNvalue, ""); n++;
        G_ipDialog = XtCreateManagedWidget("networkDialog", dialogWidgetClass,
                                            G_ipShell, args, n);
        XawDialogAddButton(G_ipDialog, "Connect", NetworkDialogConnect, NULL);
        XawDialogAddButton(G_ipDialog, "Cancel", NetworkDialogCancel, NULL);
    }

    XtPopup(G_ipShell, XtGrabExclusive);
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
    Widget form, btnServer, btnNetwork, btnSingle;
    Arg args[10];
    int n;
    char *aaPath, *serverPath, *webviewPath, *webviewResDir;
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
    webviewPath = ArgValue(argc, argv, "--webview-path");
    webviewResDir = ArgValue(argc, argv, "--webview-res-dir");
    G_port = ArgValue(argc, argv, "--port");

    G_aaBinary = aaPath ? strdup(aaPath) : DefaultBinary("AA");
    G_serverBinary = serverPath ? strdup(serverPath) : DefaultBinary("AAServer");
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
