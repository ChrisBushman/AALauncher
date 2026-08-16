/* AALauncher.r -- resources for the Mac OS 9 AALauncher.
 * Compiled with Rez (e.g. Rez -i /Developer/Headers/FlatCarbon -o AALauncher.rsrc AALauncher.r)
 * and added to the CodeWarrior project. Replaces the stationery's SimpleAlert.rsrc.
 */
#include "Types.r"
#include "SysTypes.r"

/* Alert used by the launcher's "could not launch" message (StopAlert 128).
   The DITL has a ^0 StaticText that ParamText fills in with the real text. */
resource 'ALRT' (128) {
    {60, 60, 190, 400},
    128,
    {
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent
    },
    alertPositionMainScreen
};

resource 'DITL' (128) {
    {
        /* [1] OK button (default) */
        {100, 260, 120, 320}, Button { enabled, "OK" };
        /* [2] message -- ParamText ^0; left of the alert icon area */
        {10, 70, 90, 330}, StaticText { disabled, "^0" };
    }
};

/* Memory partition (classic apps need one; the launcher is lightweight). */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    notHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    4194304,    /* preferred: 4 MB */
    2097152     /* minimum:   2 MB */
};

resource 'vers' (1) {
    0x01, 0x00, release, 0x00,
    verUS,
    "1.0",
    "AALauncher 1.0 for Mac OS 9"
};

/* ---- Display Settings dialog (writes resolution.ini) -------------------- */
resource 'DLOG' (129) {
    {80, 80, 80 + 340, 80 + 360},
    movableDBoxProc, invisible, goAway, 0x0,
    129,
    "Display Settings",
    alertPositionMainScreen
};

resource 'DITL' (129) {
    {   /* Mac Rect order is {top, left, bottom, right} */
        /* [1] OK */          {302, 280, 326, 345}, Button { enabled, "OK" };
        /* [2] Cancel */      {302, 195, 326, 265}, Button { enabled, "Cancel" };
        /* [3] Display: */    {10, 12, 26, 200},    StaticText { disabled, "Display:" };
        /* [4] Windowed */    {28, 20, 44, 150},    RadioButton { enabled, "Windowed" };
        /* [5] Fullscreen */  {28, 160, 44, 300},   RadioButton { enabled, "Fullscreen" };
        /* [6] Color Depth:*/ {52, 12, 68, 200},    StaticText { disabled, "Color Depth:" };
        /* [7] Auto */        {70, 20, 86, 120},    RadioButton { enabled, "Auto" };
        /* [8] 8-bit */       {88, 20, 104, 120},   RadioButton { enabled, "8-bit" };
        /* [9] 16-bit */      {70, 130, 86, 220},   RadioButton { enabled, "16-bit" };
        /* [10] 24-bit */     {88, 130, 104, 220},  RadioButton { enabled, "24-bit" };
        /* [11] 32-bit */     {70, 230, 86, 330},   RadioButton { enabled, "32-bit" };
        /* [12] Detail: */    {112, 12, 128, 340},  StaticText { disabled, "Level of Detail (sets window size):" };
        /* [13] 1 */          {130, 20, 146, 210},  RadioButton { enabled, "1 (fastest, 640x480)" };
        /* [14] 2 */          {148, 20, 164, 210},  RadioButton { enabled, "2 (1024x768)" };
        /* [15] 3 */          {166, 20, 182, 210},  RadioButton { enabled, "3 (1280x960)" };
        /* [16] 4 */          {130, 220, 146, 340}, RadioButton { enabled, "4" };
        /* [17] 5 */          {148, 220, 164, 340}, RadioButton { enabled, "5" };
        /* [18] 6 */          {166, 220, 182, 340}, RadioButton { enabled, "6 (sharpest)" };
        /* [19] vsync */      {190, 20, 206, 340},  CheckBox { enabled, "Vertical sync (caps FPS to refresh rate)" };
        /* [20] Renderer: */  {214, 12, 230, 200},  StaticText { disabled, "3D Renderer:" };
        /* [21] Software */   {232, 20, 248, 170},  RadioButton { enabled, "Software" };
        /* [22] Hardware */   {232, 180, 248, 345}, RadioButton { enabled, "Hardware (RAVE)" };
        /* [23] note */       {258, 12, 290, 340},  StaticText { disabled, "Applied next time you start a game." };
    }
};

/* ---- Network Game dialog (writes aanet.ini, then launches AA) ----------- */
resource 'DLOG' (130) {
    {90, 90, 90 + 140, 90 + 380},
    movableDBoxProc, invisible, goAway, 0x0,
    130,
    "Play Network Game",
    alertPositionMainScreen
};

resource 'DITL' (130) {
    {   /* {top, left, bottom, right} */
        /* [1] Connect */  {104, 290, 128, 365}, Button { enabled, "Connect" };
        /* [2] Cancel */   {104, 200, 128, 275}, Button { enabled, "Cancel" };
        /* [3] host lbl */ {14, 15, 30, 360},    StaticText { disabled, "Server host or IP address:" };
        /* [4] host edit */{34, 15, 50, 365},    EditText  { enabled, "" };
        /* [5] port lbl */ {64, 15, 80, 60},     StaticText { disabled, "Port:" };
        /* [6] port edit */{62, 62, 78, 140},    EditText  { enabled, "" };
    }
};
