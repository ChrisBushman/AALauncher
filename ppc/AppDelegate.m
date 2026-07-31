#import "AppDelegate.h"
#import "DisplaySettingsController.h"
#import "NetworkIPController.h"
#import "ScriptCompilerController.h"

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
   scriptCompilerBinary:(NSString *)aScriptCompilerBinary
                   port:(NSString *)aPort
{
    self = [super init];
    if (self)  {
        aaBinary = [([aAaBinary length] ? aAaBinary : DefaultBinary(@"AA")) retain];
        serverBinary = [([aServerBinary length] ? aServerBinary : DefaultBinary(@"AAServer")) retain];
        scriptCompilerBinary = [([aScriptCompilerBinary length] ? aScriptCompilerBinary : DefaultBinary(@"AAScriptCompiler")) retain];
        // Applied to portField's initial text once it exists -- see applicationDidFinishLaunching.
        initialPort = [([aPort length] ? aPort : @"21300") retain];
    }
    return self;
}

- (void)dealloc
{
    [aaBinary release];
    [serverBinary release];
    [scriptCompilerBinary release];
    [initialPort release];
    [scriptCompilerController release];
    [super dealloc];
}

- (void)setupMenuBar
{
    /* No nib/MainMenu.xib in this app, so no menu bar exists unless built
       here in code. Kept minimal -- just the standard app menu (for Quit,
       which OS X expects every app to offer) plus an Options menu for
       Display Settings. */
    NSMenu *mainMenu = [[[NSMenu alloc] init] autorelease];

    NSMenuItem *appMenuItem = [[[NSMenuItem alloc] init] autorelease];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [[[NSMenu alloc] init] autorelease];
    NSMenuItem *quitItem = [[[NSMenuItem alloc] initWithTitle:@"Quit AALauncher"
                                                        action:@selector(exitApplication:)
                                                 keyEquivalent:@"q"] autorelease];
    [quitItem setTarget:self];
    [appMenu addItem:quitItem];
    [appMenuItem setSubmenu:appMenu];

    NSMenuItem *optionsMenuItem = [[[NSMenuItem alloc] init] autorelease];
    [mainMenu addItem:optionsMenuItem];
    NSMenu *optionsMenu = [[[NSMenu alloc] initWithTitle:@"Options"] autorelease];
    NSMenuItem *displaySettingsItem = [[[NSMenuItem alloc] initWithTitle:@"Display Settings..."
                                                                    action:@selector(openDisplaySettings:)
                                                             keyEquivalent:@""] autorelease];
    [displaySettingsItem setTarget:self];
    [optionsMenu addItem:displaySettingsItem];
    [optionsMenuItem setSubmenu:optionsMenu];

    [NSApp setMainMenu:mainMenu];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self setupMenuBar];

    NSRect frame = NSMakeRect(0, 0, 1049, 622);
    window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [window setTitle:@"Amulets & Armor PowerPC Launcher v1.00"];
    [window center];

    NSView *content = [window contentView];

    /* Button bar: bottom 160px of the window (Cocoa's default view
       coordinate origin is bottom-left, so this sits at y=0..160). Web
       view fills the remaining space above it. Two rows: the original 3
       buttons sit at y=81 (21px below the web view, same as the original
       single-row 100px-bar layout), and a second row below at y~16-44
       holds the server port field plus the script compiler / exit
       buttons. */
    webView = [[WebView alloc] initWithFrame:NSMakeRect(0, 160, 1049, 462) frameName:nil groupName:nil];
    [content addSubview:webView];
    [[webView mainFrame] loadRequest:
        [NSURLRequest requestWithURL:
            [NSURL URLWithString:@"http://www.amuletsandarmor.com/index.htm?launcher=1&classic=1"]]];

    NSFont *boldFont = [NSFont boldSystemFontOfSize:13];

    NSButton *btnServer = [[[NSButton alloc] initWithFrame:NSMakeRect(148, 81, 234, 58)] autorelease];
    [btnServer setTitle:@"Start A&A Server"];
    [btnServer setFont:boldFont];
    [btnServer setBezelStyle:NSRoundedBezelStyle];
    [btnServer setTarget:self];
    [btnServer setAction:@selector(startServer:)];
    [content addSubview:btnServer];

    NSButton *btnNetwork = [[[NSButton alloc] initWithFrame:NSMakeRect(406, 81, 234, 58)] autorelease];
    [btnNetwork setTitle:@"Play Network Game"];
    [btnNetwork setFont:boldFont];
    [btnNetwork setBezelStyle:NSRoundedBezelStyle];
    [btnNetwork setTarget:self];
    [btnNetwork setAction:@selector(playNetwork:)];
    [content addSubview:btnNetwork];

    NSButton *btnSingle = [[[NSButton alloc] initWithFrame:NSMakeRect(665, 81, 234, 58)] autorelease];
    [btnSingle setTitle:@"Play Single Player"];
    [btnSingle setFont:boldFont];
    [btnSingle setBezelStyle:NSRoundedBezelStyle];
    [btnSingle setTarget:self];
    [btnSingle setAction:@selector(playSinglePlayer:)];
    [content addSubview:btnSingle];

    NSTextField *portLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(148, 22, 90, 18)] autorelease];
    [portLabel setStringValue:@"Server Port:"];
    [portLabel setEditable:NO];
    [portLabel setSelectable:NO];
    [portLabel setBezeled:NO];
    [portLabel setDrawsBackground:NO];
    [content addSubview:portLabel];

    portField = [[NSTextField alloc] initWithFrame:NSMakeRect(240, 18, 70, 22)];
    [portField setStringValue:initialPort];
    [content addSubview:portField];

    NSButton *btnScriptCompiler = [[[NSButton alloc] initWithFrame:NSMakeRect(406, 16, 234, 30)] autorelease];
    [btnScriptCompiler setTitle:@"Script Compiler"];
    [btnScriptCompiler setBezelStyle:NSRoundedBezelStyle];
    [btnScriptCompiler setTarget:self];
    [btnScriptCompiler setAction:@selector(openScriptCompiler:)];
    [content addSubview:btnScriptCompiler];

    NSButton *btnExit = [[[NSButton alloc] initWithFrame:NSMakeRect(665, 16, 234, 30)] autorelease];
    [btnExit setTitle:@"Exit"];
    [btnExit setBezelStyle:NSRoundedBezelStyle];
    [btnExit setTarget:self];
    [btnExit setAction:@selector(exitApplication:)];
    [content addSubview:btnExit];

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)launchProcess:(NSString *)binaryPath arguments:(NSArray *)args
{
    NSTask *task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:binaryPath];
    [task setArguments:args];
    NSString *workDir = [binaryPath stringByDeletingLastPathComponent];
    [task setCurrentDirectoryPath:workDir];

    /* AA/AAServer link against bundled SDL dylibs by absolute build-time
       path (see their own run.sh wrappers); dyld only finds them via
       DYLD_LIBRARY_PATH pointed at the lib/ folder shipped alongside each
       binary, since we're exec'ing them directly rather than through a
       shell wrapper. */
    NSMutableDictionary *env = [[[NSMutableDictionary alloc] initWithDictionary:[[NSProcessInfo processInfo] environment]] autorelease];
    NSString *libDir = [workDir stringByAppendingPathComponent:@"lib"];
    NSString *existing = [env objectForKey:@"DYLD_LIBRARY_PATH"];
    NSString *combined = [existing length] ? [NSString stringWithFormat:@"%@:%@", libDir, existing] : libDir;
    [env setObject:combined forKey:@"DYLD_LIBRARY_PATH"];
    [task setEnvironment:env];

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
    NSString *portValue = [[portField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    /* --console: launchProcess uses NSTask, which leaves AAServer with no
       controlling terminal at all -- with no flag, its output goes
       nowhere and it shows up as an anonymous background process with no
       visible window. The flag tells AAServer to relaunch itself into a
       real Terminal.app window; harmless no-op on builds where a console
       already exists. */
    NSMutableArray *args = [NSMutableArray arrayWithObject:@"--console"];
    if ([portValue length])
        [args addObject:portValue];
    [self launchProcess:serverBinary arguments:args];
}

- (void)playNetwork:(id)sender
{
    NetworkIPController *ipController = [[[NetworkIPController alloc] init] autorelease];
    NSString *ip = [ipController runModal];
    if (![ip length])
        return;
    NSString *ipPort = [ipController selectedPort];

    NSMutableArray *args = [NSMutableArray arrayWithObject:ip];
    if ([ipPort length])
        [args addObject:ipPort];
    if ([self launchProcess:aaBinary arguments:args])
        [NSApp terminate:nil];
}

- (void)playSinglePlayer:(id)sender
{
    if ([self launchProcess:aaBinary arguments:[NSArray array]])
        [NSApp terminate:nil];
}

- (void)openScriptCompiler:(id)sender
{
    if (scriptCompilerController == nil)
        scriptCompilerController = [[ScriptCompilerController alloc] initWithCompilerPath:scriptCompilerBinary];
    [scriptCompilerController showWindow];
}

- (void)openDisplaySettings:(id)sender
{
    DisplaySettingsController *controller = [[[DisplaySettingsController alloc] initWithAABinary:aaBinary] autorelease];
    [controller runModal];
}

- (void)exitApplication:(id)sender
{
    [NSApp terminate:nil];
}

@end
