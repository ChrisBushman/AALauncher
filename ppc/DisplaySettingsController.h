#import <Cocoa/Cocoa.h>

/* "Display Settings" dialog -- Fullscreen/Windowed, Color Depth, and Level
   of Detail, writing directly to resolution.ini (read by AA.exe's RESSCALE
   module, see AmuletsArmor's Source/Win32/RESSCALE.C) in the same
   directory as the AA binary this launcher starts. Same 3 settings and
   same resolution.ini format as the other launchers (see
   mac/DisplaySettingsDialog.cpp and win9x/main.c). Run via -runModal. */
@interface DisplaySettingsController : NSObject
{
    NSWindow *panel;
    NSString *aaBinary;

    NSButton *windowedRadio;
    NSButton *fullscreenRadio;

    NSButton *bppAutoRadio;
    NSButton *bpp8Radio;
    NSButton *bpp16Radio;
    NSButton *bpp24Radio;
    NSButton *bpp32Radio;

    NSButton *detailRadios[6];
}

- (id)initWithAABinary:(NSString *)anAaBinary;
- (void)runModal;

- (void)onDisplayRadioClicked:(id)sender;
- (void)onBppRadioClicked:(id)sender;
- (void)onDetailRadioClicked:(id)sender;

@end
