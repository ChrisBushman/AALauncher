/* Amulets & Armor IRIX Launcher -- plain Xt/Xaw native UI (no web view;
 * see README.md for why: no embeddable browser exists for this platform).
 * Behavior matches the Windows/Qt/Cocoa launchers: 3 buttons that spawn
 * AA/AAServer as detached subprocesses, plus a small modal dialog to
 * enter a server IP for "Play Network Game". Xt + Xaw only -- both are
 * part of any IRIX X11R6 install, no Nekoware/TGCware dependency. */
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

static XtAppContext G_appContext;
static Widget G_topLevel;
static Visual *G_visual = NULL;
static int G_depth = 0;
static char *G_selfDir;
static char *G_aaBinary;
static char *G_serverBinary;
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
static void LaunchProcess(const char *binaryPath, char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0)  {
        /* Child: detach into its own session so it survives the
           launcher exiting, then exec. */
        setsid();
        chdir(G_selfDir);
        execv(binaryPath, argv);
        /* execv only returns on failure. */
        _exit(127);
    } else if (pid < 0)  {
        ShowMessage("Launch Error", "Failed to fork a new process.");
    }
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
/* Title screen image, above the button row. No web view exists for this
   platform (see README.md), so a static piece of the game's own title
   art stands in for it -- embedded as raw RGB (title_image.h) rather
   than decoded from a real image file at runtime, so this has no image-
   library dependency at all. */
static Widget G_imageArea;
static XImage *G_titleXImage = NULL;

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

static void ImageAreaExpose(Widget w, XtPointer clientData, XEvent *event, Boolean *cont)
{
    Display *dpy;
    Window win;
    GC gc;

    if (event->type != Expose || event->xexpose.count != 0)
        return;

    BuildTitleXImage(w);

    dpy = XtDisplay(w);
    win = XtWindow(w);
    gc = XCreateGC(dpy, win, 0, NULL);
    XPutImage(dpy, win, gc, G_titleXImage, 0, 0, 0, 0,
              TITLE_IMAGE_WIDTH, TITLE_IMAGE_HEIGHT);
    XFreeGC(dpy, gc);
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
    char *aaPath, *serverPath;
    Display *dpy;
    XVisualInfo vinfo;

    /* This display's default visual is 8-bit PseudoColor (confirmed via
       xdpyinfo on the O2) -- indexed color with no real RGB masks, which
       is why a straightforward RGB-mask pixel-packing approach (see
       ComponentToMask/BuildTitleXImage) renders solid black there: every
       mask is 0. There's a second 8-bit visual, StaticColor, with a
       real (if coarse, 3-3-2) fixed RGB-to-pixel mapping -- request it
       explicitly for the whole app up front, since Xt widgets don't
       care what visual they're drawn on. Falls back to the default
       visual if StaticColor genuinely isn't available (e.g. testing
       under XQuartz, which is TrueColor already and doesn't need this
       at all). */
    XtToolkitInitialize();
    G_appContext = XtCreateApplicationContext();
    dpy = XtOpenDisplay(G_appContext, NULL, "AALauncher", "AALauncher", NULL, 0, &argc, argv);
    if (dpy == NULL)  {
        fprintf(stderr, "AALauncher: cannot open display\n");
        exit(1);
    }

    if (XMatchVisualInfo(dpy, DefaultScreen(dpy), 8, StaticColor, &vinfo))  {
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
    G_port = ArgValue(argc, argv, "--port");

    G_aaBinary = aaPath ? strdup(aaPath) : DefaultBinary("AA");
    G_serverBinary = serverPath ? strdup(serverPath) : DefaultBinary("AAServer");

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

        /* Image first (left-anchored provisionally -- corrected below)
           with the 3 buttons below it via fromVert. */
        n = 0;
        XtSetArg(args[n], XtNwidth, TITLE_IMAGE_WIDTH); n++;
        XtSetArg(args[n], XtNheight, TITLE_IMAGE_HEIGHT); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        G_imageArea = XtCreateManagedWidget("imageArea", widgetClass, form, args, n);
        XtAddEventHandler(G_imageArea, ExposureMask, False, ImageAreaExpose, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Start A&A Server"); n++;
        XtSetArg(args[n], XtNfromVert, G_imageArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNleft, XtChainLeft); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        btnServer = XtCreateManagedWidget("btnServer", commandWidgetClass, form, args, n);
        XtAddCallback(btnServer, XtNcallback, StartServerCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Play Network Game"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnServer); n++;
        XtSetArg(args[n], XtNfromVert, G_imageArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        btnNetwork = XtCreateManagedWidget("btnNetwork", commandWidgetClass, form, args, n);
        XtAddCallback(btnNetwork, XtNcallback, PlayNetworkCallback, NULL);

        n = 0;
        XtSetArg(args[n], XtNlabel, "Play Single Player"); n++;
        XtSetArg(args[n], XtNfromHoriz, btnNetwork); n++;
        XtSetArg(args[n], XtNfromVert, G_imageArea); n++;
        XtSetArg(args[n], XtNtop, XtChainTop); n++;
        XtSetArg(args[n], XtNbackground, bgPixel); n++;
        XtSetArg(args[n], XtNforeground, fgPixel); n++;
        btnSingle = XtCreateManagedWidget("btnSingle", commandWidgetClass, form, args, n);
        XtAddCallback(btnSingle, XtNcallback, PlaySinglePlayerCallback, NULL);
    }

    /* Form widgets don't resolve constraint-based child positions until
       they're actually realized (querying XtNx/XtNwidth beforehand can
       read as 0/unset -- confirmed differing between XQuartz, where it
       happened to already work, and IRIX's older Xaw, where it didn't).
       Realize first with the image provisionally left-anchored, then
       measure the real button-row width and correct it -- Form reflows
       immediately in response to a post-realize XtSetValues. */
    XtRealizeWidget(G_topLevel);

    {
        Position singleX = 0;
        Dimension singleWidth = 0;
        Position imageX;

        n = 0;
        XtSetArg(args[n], XtNx, &singleX); n++;
        XtSetArg(args[n], XtNwidth, &singleWidth); n++;
        XtGetValues(btnSingle, args, n);

        imageX = (singleX + (Position)singleWidth - TITLE_IMAGE_WIDTH) / 2;
        if (imageX < 0)
            imageX = 0;

        n = 0;
        XtSetArg(args[n], XtNhorizDistance, imageX); n++;
        XtSetValues(G_imageArea, args, n);
    }
    XtAppMainLoop(G_appContext);

    return 0;
}
