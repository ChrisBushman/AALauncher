#import "AppDelegate.h"
#import "NetworkIPController.h"

static NSString *DefaultBinary(NSString *name)
{
    /* NSBundle's executablePath (not argv[0], which may be a relative
       path depending on how this was invoked) reliably resolves to this
       binary's own absolute location, even for a bare command-line
       executable with no real .app bundle structure. */
    NSString *dir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    return [dir stringByAppendingPathComponent:name];
}

@interface AppDelegate (Private)
- (BOOL)launchProcess:(NSString *)binaryPath arguments:(NSArray *)args;
@end

@implementation AppDelegate

- (id)initWithAABinary:(NSString *)aAaBinary
           serverBinary:(NSString *)aServerBinary
                   port:(NSString *)aPort
{
    self = [super init];
    if (self)  {
        aaBinary = [([aAaBinary length] ? aAaBinary : DefaultBinary(@"AA")) retain];
        serverBinary = [([aServerBinary length] ? aServerBinary : DefaultBinary(@"AAServer")) retain];
        port = [aPort retain];
    }
    return self;
}

- (void)dealloc
{
    [aaBinary release];
    [serverBinary release];
    [port release];
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect frame = NSMakeRect(0, 0, 1049, 562);
    window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [window setTitle:@"Amulets & Armor PowerPC Launcher v1.00"];
    [window center];

    NSView *content = [window contentView];

    /* Button bar: bottom 100px of the window (Cocoa's default view
       coordinate origin is bottom-left, so this sits at y=0..100). Web
       view fills the remaining space above it. Button positions/sizes
       match the Windows/Qt reference layout exactly -- its 21px top
       margin and 21px bottom margin within a 100px bar happen to be
       numerically identical once converted to Cocoa's bottom-up
       coordinates (100 - 21 - 58 = 21), so the same y=21 constant works
       in both coordinate systems. */
    webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 100, 1049, 462) frameName:nil groupName:nil];
    [content addSubview:webView];
    [[webView mainFrame] loadRequest:
        [NSURLRequest requestWithURL:
            [NSURL URLWithString:@"http://www.amuletsandarmor.com/index.htm?launcher=1&classic=1"]]];

    NSFont *boldFont = [NSFont boldSystemFontOfSize:13];

    NSButton *btnServer = [[[NSButton alloc] initWithFrame:NSMakeRect(148, 21, 234, 58)] autorelease];
    [btnServer setTitle:@"Start A&A Server"];
    [btnServer setFont:boldFont];
    [btnServer setBezelStyle:NSRoundedBezelStyle];
    [btnServer setTarget:self];
    [btnServer setAction:@selector(startServer:)];
    [content addSubview:btnServer];

    NSButton *btnNetwork = [[[NSButton alloc] initWithFrame:NSMakeRect(406, 21, 234, 58)] autorelease];
    [btnNetwork setTitle:@"Play Network Game"];
    [btnNetwork setFont:boldFont];
    [btnNetwork setBezelStyle:NSRoundedBezelStyle];
    [btnNetwork setTarget:self];
    [btnNetwork setAction:@selector(playNetwork:)];
    [content addSubview:btnNetwork];

    NSButton *btnSingle = [[[NSButton alloc] initWithFrame:NSMakeRect(665, 21, 234, 58)] autorelease];
    [btnSingle setTitle:@"Play Single Player"];
    [btnSingle setFont:boldFont];
    [btnSingle setBezelStyle:NSRoundedBezelStyle];
    [btnSingle setTarget:self];
    [btnSingle setAction:@selector(playSinglePlayer:)];
    [content addSubview:btnSingle];

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)launchProcess:(NSString *)binaryPath arguments:(NSArray *)args
{
    NSTask *task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:binaryPath];
    [task setArguments:args];
    [task setCurrentDirectoryPath:[binaryPath stringByDeletingLastPathComponent]];
    @try  {
        [task launch];
    }
    @catch (NSException *e)  {
        NSAlert *alert = [NSAlert alertWithMessageText:@"Launch Error"
                                          defaultButton:@"OK"
                                        alternateButton:nil
                                            otherButton:nil
                              informativeTextWithFormat:@"Failed to launch: %@ (%@)", binaryPath, [e reason]];
        [alert runModal];
        return NO;
    }
    return YES;
}

- (void)startServer:(id)sender
{
    NSMutableArray *args = [NSMutableArray array];
    if ([port length])
        [args addObject:port];
    [self launchProcess:serverBinary arguments:args];
}

- (void)playNetwork:(id)sender
{
    NetworkIPController *ipController = [[[NetworkIPController alloc] init] autorelease];
    NSString *ip = [ipController runModal];
    if (![ip length])
        return;

    NSMutableArray *args = [NSMutableArray arrayWithObject:ip];
    if ([port length])
        [args addObject:port];
    if ([self launchProcess:aaBinary arguments:args])
        [NSApp terminate:nil];
}

- (void)playSinglePlayer:(id)sender
{
    if ([self launchProcess:aaBinary arguments:[NSArray array]])
        [NSApp terminate:nil];
}

@end
