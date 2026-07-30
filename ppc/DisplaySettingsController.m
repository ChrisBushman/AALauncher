#import "DisplaySettingsController.h"

@interface DisplaySettingsController (Private)
- (NSString *)iniPath;
- (void)loadFromIni;
- (void)saveToIni;
- (void)onOk:(id)sender;
- (void)onCancel:(id)sender;
- (NSButton *)radioWithFrame:(NSRect)frame title:(NSString *)title superview:(NSView *)superview action:(SEL)action;
@end

@implementation DisplaySettingsController

- (id)initWithAABinary:(NSString *)anAaBinary
{
    self = [super init];
    if (self)  {
        aaBinary = [anAaBinary retain];

        NSRect frame = NSMakeRect(0, 0, 360, 470);
        panel = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSTitledWindowMask | NSClosableWindowMask)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
        [panel setTitle:@"Display Settings"];
        [panel setReleasedWhenClosed:NO];

        NSView *content = [panel contentView];

        /* Display box: y=396..452 */
        NSBox *displayBox = [[[NSBox alloc] initWithFrame:NSMakeRect(16, 396, 328, 56)] autorelease];
        [displayBox setTitle:@"Display"];
        [content addSubview:displayBox];
        NSView *displayContent = [displayBox contentView];
        windowedRadio = [self radioWithFrame:NSMakeRect(12, 6, 140, 20) title:@"Windowed" superview:displayContent action:@selector(onDisplayRadioClicked:)];
        fullscreenRadio = [self radioWithFrame:NSMakeRect(160, 6, 140, 20) title:@"Fullscreen" superview:displayContent action:@selector(onDisplayRadioClicked:)];

        /* Color Depth box: y=286..386 */
        NSBox *bppBox = [[[NSBox alloc] initWithFrame:NSMakeRect(16, 286, 328, 100)] autorelease];
        [bppBox setTitle:@"Color Depth"];
        [content addSubview:bppBox];
        NSView *bppContent = [bppBox contentView];
        bppAutoRadio = [self radioWithFrame:NSMakeRect(12, 58, 200, 20) title:@"Auto (recommended)" superview:bppContent action:@selector(onBppRadioClicked:)];
        bpp8Radio = [self radioWithFrame:NSMakeRect(12, 36, 90, 20) title:@"8-bit" superview:bppContent action:@selector(onBppRadioClicked:)];
        bpp16Radio = [self radioWithFrame:NSMakeRect(110, 36, 90, 20) title:@"16-bit" superview:bppContent action:@selector(onBppRadioClicked:)];
        bpp24Radio = [self radioWithFrame:NSMakeRect(12, 12, 90, 20) title:@"24-bit" superview:bppContent action:@selector(onBppRadioClicked:)];
        bpp32Radio = [self radioWithFrame:NSMakeRect(110, 12, 90, 20) title:@"32-bit" superview:bppContent action:@selector(onBppRadioClicked:)];

        /* Level of Detail box: y=76..276 (also sets window size, see saveToIni) */
        NSBox *detailBox = [[[NSBox alloc] initWithFrame:NSMakeRect(16, 76, 328, 200)] autorelease];
        [detailBox setTitle:@"Level of Detail (also sets window size)"];
        [content addSubview:detailBox];
        NSView *detailContent = [detailBox contentView];
        NSString *detailLabels[6] = {
            @"1 (fastest, 640x480)",
            @"2 (1024x768)",
            @"3 (1280x960)",
            @"4 (1280x960)",
            @"5 (1280x960)",
            @"6 (sharpest, 1280x960)",
        };
        int i;
        for (i = 0; i < 6; i++)  {
            NSRect r = NSMakeRect(12, 6 + (5 - i) * 26, 300, 20);
            detailRadios[i] = [self radioWithFrame:r title:detailLabels[i] superview:detailContent action:@selector(onDetailRadioClicked:)];
        }

        NSTextField *note = [[[NSTextField alloc] initWithFrame:NSMakeRect(16, 46, 328, 18)] autorelease];
        [note setStringValue:@"Applied the next time you start a game."];
        [note setEditable:NO];
        [note setSelectable:NO];
        [note setBezeled:NO];
        [note setDrawsBackground:NO];
        [note setFont:[NSFont systemFontOfSize:11]];
        [content addSubview:note];

        NSButton *cancelButton = [[[NSButton alloc] initWithFrame:NSMakeRect(180, 12, 80, 26)] autorelease];
        [cancelButton setTitle:@"Cancel"];
        [cancelButton setBezelStyle:NSRoundedBezelStyle];
        [cancelButton setTarget:self];
        [cancelButton setAction:@selector(onCancel:)];
        [content addSubview:cancelButton];

        NSButton *okButton = [[[NSButton alloc] initWithFrame:NSMakeRect(264, 12, 80, 26)] autorelease];
        [okButton setTitle:@"OK"];
        [okButton setBezelStyle:NSRoundedBezelStyle];
        [okButton setTarget:self];
        [okButton setAction:@selector(onOk:)];
        [panel setDefaultButtonCell:[okButton cell]];
        [content addSubview:okButton];

        [self loadFromIni];
    }
    return self;
}

- (void)dealloc
{
    [aaBinary release];
    [panel release];
    [super dealloc];
}

- (NSButton *)radioWithFrame:(NSRect)frame title:(NSString *)title superview:(NSView *)superview action:(SEL)action
{
    NSButton *radio = [[[NSButton alloc] initWithFrame:frame] autorelease];
    [radio setButtonType:NSRadioButton];
    [radio setTitle:title];
    /* AppKit's documented same-superview radio auto-exclusion turned out
       not to reliably apply here (confirmed on real PPC/Tiger hardware --
       a newly-clicked radio wouldn't turn its sibling off, so saveToIni's
       "first one still on" scan kept finding the ORIGINAL loadFromIni
       selection instead of the user's new pick). Every action below
       enforces exclusivity explicitly instead of depending on that
       mechanism. */
    [radio setTarget:self];
    [radio setAction:action];
    [superview addSubview:radio];
    return radio;
}

- (void)onDisplayRadioClicked:(id)sender
{
    [windowedRadio setState:(sender == windowedRadio) ? NSOnState : NSOffState];
    [fullscreenRadio setState:(sender == fullscreenRadio) ? NSOnState : NSOffState];
}

- (void)onBppRadioClicked:(id)sender
{
    NSButton *radios[5] = { bppAutoRadio, bpp8Radio, bpp16Radio, bpp24Radio, bpp32Radio };
    int i;
    for (i = 0; i < 5; i++)
        [radios[i] setState:(sender == radios[i]) ? NSOnState : NSOffState];
}

- (void)onDetailRadioClicked:(id)sender
{
    int i;
    for (i = 0; i < 6; i++)
        [detailRadios[i] setState:(sender == detailRadios[i]) ? NSOnState : NSOffState];
}

- (NSString *)iniPath
{
    NSString *dir = [aaBinary stringByDeletingLastPathComponent];
    return [dir stringByAppendingPathComponent:@"resolution.ini"];
}

- (void)loadFromIni
{
    /* Defaults match RESSCALE.C's own compiled-in defaults (fullscreen=1,
       bpp=0/auto, detail=2) so this dialog shows the same choice AA.exe
       itself would default to before resolution.ini exists. */
    int fullscreen = 1;
    int bpp = 0;
    int detail = 2;

    NSString *contents = [NSString stringWithContentsOfFile:[self iniPath]];
    if (contents != nil)  {
        NSArray *lines = [contents componentsSeparatedByString:@"\n"];
        unsigned int i, count = [lines count];
        for (i = 0; i < count; i++)  {
            NSString *line = [[lines objectAtIndex:i] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            NSRange eq = [line rangeOfString:@"="];
            if (eq.location == NSNotFound || eq.location == 0)
                continue;
            NSString *key = [line substringToIndex:eq.location];
            NSString *value = [line substringFromIndex:(eq.location + 1)];
            int intValue = [value intValue];
            if ([key isEqualToString:@"fullscreen"])
                fullscreen = intValue;
            else if ([key isEqualToString:@"bpp"])
                bpp = intValue;
            else if ([key isEqualToString:@"detail"])
                detail = intValue;
        }
    }

    [windowedRadio setState:(fullscreen ? NSOffState : NSOnState)];
    [fullscreenRadio setState:(fullscreen ? NSOnState : NSOffState)];

    NSButton *bppRadio = bppAutoRadio;
    switch (bpp)  {
        case 8:  bppRadio = bpp8Radio;  break;
        case 16: bppRadio = bpp16Radio; break;
        case 24: bppRadio = bpp24Radio; break;
        case 32: bppRadio = bpp32Radio; break;
        default: bppRadio = bppAutoRadio; break;
    }
    {
        NSButton *bppRadios[5] = { bppAutoRadio, bpp8Radio, bpp16Radio, bpp24Radio, bpp32Radio };
        int i;
        for (i = 0; i < 5; i++)
            [bppRadios[i] setState:(bppRadios[i] == bppRadio) ? NSOnState : NSOffState];
    }

    if (detail < 1) detail = 1;
    if (detail > 6) detail = 6;
    {
        int i;
        for (i = 0; i < 6; i++)
            [detailRadios[i] setState:(i == detail - 1) ? NSOnState : NSOffState];
    }
}

- (void)saveToIni
{
    /* Rewrites resolution.ini with the current selections, preserving
       every other line (comments, scale=/fit=/aspect=/hotkeys=) exactly
       as found -- same merge-preserve approach as the other launchers'
       ini writers, for the same reason: an explicit width=/height=
       otherwise pins the window regardless of detail level (RESSCALE.C's
       IComputeWindowSize gives explicit width=/height= priority over
       scale=), so window size needs to be rewritten alongside detail
       level, not left stale. */
    int fullscreen = ([fullscreenRadio state] == NSOnState) ? 1 : 0;

    int bpp = 0;
    if ([bpp8Radio state] == NSOnState) bpp = 8;
    else if ([bpp16Radio state] == NSOnState) bpp = 16;
    else if ([bpp24Radio state] == NSOnState) bpp = 24;
    else if ([bpp32Radio state] == NSOnState) bpp = 32;

    int detail = 2;
    int i;
    for (i = 0; i < 6; i++)  {
        if ([detailRadios[i] state] == NSOnState)  {
            detail = i + 1;
            break;
        }
    }

    /* Window size tier per detail level -- capped at the level-3 size for
       levels 4-6 rather than continuing to grow the window: beyond that
       point higher detail is about render sharpness, not needing an even
       bigger window. All 4:3, matching resolution.ini's own default
       aspect. */
    static const int widths[6]  = { 640, 1024, 1280, 1280, 1280, 1280 };
    static const int heights[6] = { 480,  768,  960,  960,  960,  960 };
    int width = widths[detail - 1];
    int height = heights[detail - 1];

    NSMutableString *newContents = [NSMutableString string];
    NSString *existing = [NSString stringWithContentsOfFile:[self iniPath]];
    if (existing != nil)  {
        NSArray *lines = [existing componentsSeparatedByString:@"\n"];
        unsigned int j, count = [lines count];
        for (j = 0; j < count; j++)  {
            NSString *line = [lines objectAtIndex:j];
            NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            BOOL isManaged =
                [trimmed hasPrefix:@"fullscreen="] ||
                [trimmed hasPrefix:@"bpp="] ||
                [trimmed hasPrefix:@"detail="] ||
                [trimmed hasPrefix:@"width="] ||
                [trimmed hasPrefix:@"height="];
            if (!isManaged && (j < count - 1 || [trimmed length] > 0))  {
                [newContents appendString:line];
                [newContents appendString:@"\n"];
            }
        }
    }

    [newContents appendFormat:@"fullscreen=%d\n", fullscreen];
    [newContents appendFormat:@"bpp=%d\n", bpp];
    [newContents appendFormat:@"detail=%d\n", detail];
    [newContents appendFormat:@"width=%d\n", width];
    [newContents appendFormat:@"height=%d\n", height];

    [newContents writeToFile:[self iniPath] atomically:YES];
}

- (void)onOk:(id)sender
{
    [self saveToIni];
    [NSApp stopModal];
}

- (void)onCancel:(id)sender
{
    [NSApp stopModal];
}

- (BOOL)windowShouldClose:(id)sender
{
    [NSApp stopModal];
    return YES;
}

- (void)runModal
{
    [panel center];
    [panel setDelegate:(id)self];
    [NSApp runModalForWindow:panel];
    [panel close];
}

@end
