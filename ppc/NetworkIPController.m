#import "NetworkIPController.h"

/* No formal NSTextFieldDelegate/NSWindowDelegate protocol declaration --
   neither exists on Tiger's SDK (delegates were informal protocols in
   that era: implement the method, no @protocol conformance needed or
   available). */
@interface NetworkIPController (Private)
- (void)onConnect:(id)sender;
- (void)onCancel:(id)sender;
@end

@implementation NetworkIPController

- (id)init
{
    self = [super init];
    if (self)  {
        NSRect frame = NSMakeRect(0, 0, 390, 100);
        panel = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSTitledWindowMask | NSClosableWindowMask)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
        [panel setTitle:@"AALauncher Network Connection"];
        [panel setReleasedWhenClosed:NO];
        [panel setDelegate:(id)self];

        NSView *content = [panel contentView];

        NSTextField *label = [[[NSTextField alloc] initWithFrame:NSMakeRect(16, 62, 358, 20)] autorelease];
        [label setStringValue:@"Enter Network IP of Server:"];
        [label setEditable:NO];
        [label setSelectable:NO];
        [label setBezeled:NO];
        [label setDrawsBackground:NO];
        [content addSubview:label];

        ipField = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 30, 260, 22)];
        [ipField setDelegate:(id)self];
        [content addSubview:ipField];

        connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(284, 28, 90, 26)];
        [connectButton setTitle:@"Connect"];
        [connectButton setBezelStyle:NSRoundedBezelStyle];
        [connectButton setTarget:self];
        [connectButton setAction:@selector(onConnect:)];
        [connectButton setEnabled:NO];
        [panel setDefaultButtonCell:[connectButton cell]];
        [content addSubview:connectButton];

        result = nil;
    }
    return self;
}

- (void)dealloc
{
    [ipField release];
    [connectButton release];
    [panel release];
    [result release];
    [super dealloc];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    NSString *text = [[ipField stringValue]
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    [connectButton setEnabled:([text length] > 0)];
}

- (void)onConnect:(id)sender
{
    result = [[[ipField stringValue]
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] retain];
    [NSApp stopModal];
}

- (void)onCancel:(id)sender
{
    result = nil;
    [NSApp stopModal];
}

- (BOOL)windowShouldClose:(id)sender
{
    /* The window's close button doesn't go through onCancel: -- if we
       don't stop the modal loop here too, runModalForWindow: blocks
       forever since the window would just hide, not actually close. */
    if (result == nil)
        [NSApp stopModal];
    return YES;
}

- (NSString *)runModal
{
    [panel center];
    [NSApp runModalForWindow:panel];
    [panel close];
    return [result autorelease];
}

@end
