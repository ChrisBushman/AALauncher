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
