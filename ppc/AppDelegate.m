#import "AppDelegate.h"
#import "DisplaySettingsController.h"
#import "NetworkIPController.h"
#import "ScriptCompilerController.h"
#import <stdlib.h>

static NSString *DefaultBinary(NSString *name)
{
    /* NSBundle's executablePath (not argv[0], which may be a relative
       path depending on how this was invoked) reliably resolves to this
       binary's own absolute location, even for a bare command-line
       executable with no real .app bundle structure. */
    NSString *dir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    return [dir stringByAppendingPathComponent:name];
}

/* Wraps a path/argument in single quotes for embedding in a /bin/sh -c
   script, escaping any literal single quotes it contains.
   stringByReplacingOccurrencesOfString:withString: is a 10.5+ addition --
   not available on this Tiger/10.4 target -- so this uses
   NSMutableString's replaceOccurrencesOfString:withString:options:range:
   instead, which has been part of Foundation since long before 10.4. */
static NSString *ShellQuote(NSString *s)
{
    NSMutableString *escaped = [NSMutableString stringWithString:s];
    [escaped replaceOccurrencesOfString:@"'"
                              withString:@"'\\''"
                                 options:0
                                   range:NSMakeRange(0, [escaped length])];
    return [NSString stringWithFormat:@"'%@'", escaped];
}

@interface AppDelegate (Private)
- (BOOL)launchProcess:(NSString *)binaryPath arguments:(NSArray *)args;
- (BOOL)launchProcessAfterQuit:(NSString *)binaryPath arguments:(NSArray *)args;
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

/* Real Tiger hardware: AA dies immediately on launch whenever it's spawned
   as a child of this still-running Cocoa app -- confirmed via AA's own
   inherited stderr showing "CFMessagePort: bootstrap_register(): failed"/
   "CFMessagePortCreateLocal failed" and AA exiting before it ever reaches
   its own code (no RESSCALE/version banner, no connect attempt reaching
   the server). AA inherits this launcher's Mach bootstrap namespace as an
   NSTask child, and whatever this launcher's own Cocoa/AppKit session is
   still holding in it collides with AA's own registration attempt. A
   fixed delay before this launcher calls -terminate: does NOT fix this
   (confirmed) -- the parent doesn't release its claim gradually, it holds
   it until it has *fully* exited, so any delay short of that is
   equivalent to none. Instead of guessing at a delay, exec a tiny shell
   wrapper that actively polls for this launcher's own pid to actually
   disappear before it execs AA -- by the time that's true, this process
   (and whatever it was holding) is completely gone, no race window left
   to lose. -terminate: is called immediately after this returns; the
   wrapper survives as an independent (reparented) process since exiting
   a parent doesn't kill already-launched NSTask children. */
- (BOOL)launchProcessAfterQuit:(NSString *)binaryPath arguments:(NSArray *)args
{
    pid_t myPid = [[NSProcessInfo processInfo] processIdentifier];
    NSString *workDir = [binaryPath stringByDeletingLastPathComponent];
    NSString *libDir = [workDir stringByAppendingPathComponent:@"lib"];

    NSMutableString *script = [NSMutableString string];
    [script appendFormat:@"while kill -0 %d 2>/dev/null; do sleep 0.05; done; ", (int)myPid];
    [script appendFormat:@"cd %@ || exit 1; ", ShellQuote(workDir)];
    [script appendFormat:@"DYLD_LIBRARY_PATH=%@:\"$DYLD_LIBRARY_PATH\" exec %@",
        ShellQuote(libDir), ShellQuote(binaryPath)];
    /* Classic indexed loop, not fast enumeration (for...in) -- an
       Objective-C 2.0 / 10.5+ addition not available on this Tiger/10.4
       toolchain. */
    {
        unsigned int i, count = [args count];
        for (i = 0; i < count; i++)  {
            [script appendFormat:@" %@", ShellQuote([args objectAtIndex:i])];
        }
    }

    /* AA's own stdout/stderr otherwise go nowhere visible when launched
       this way (this launcher itself has no controlling terminal when
       double-clicked, so there's nothing for a plain inherited fd to
       land in) -- capture unconditionally so any printf/DebugCheck output
       from a failure that *doesn't* produce a CrashReporter entry (a
       clean exit()/abort() from a failed assertion, as opposed to an
       actual EXC_BAD_ACCESS) is still visible after the fact. */
    [script appendString:@" > \"$HOME/Desktop/AA-console.log\" 2>&1"];

    NSTask *task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:@"/bin/sh"];
    [task setArguments:[NSArray arrayWithObjects:@"-c", script, nil]];

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
    if ([self launchProcessAfterQuit:aaBinary arguments:args])
        [self quitImmediately];
}

- (void)playSinglePlayer:(id)sender
{
    if ([self launchProcessAfterQuit:aaBinary arguments:[NSArray array]])
        [self quitImmediately];
}

/* [NSApp terminate:nil] runs this app's normal Cocoa teardown -- for a
   window closing normally (the Exit button, exitApplication: below)
   that's exactly right. But right after spawning AA to hand off to it,
   confirmed on real Tiger hardware that this same teardown crashes
   (EXC_BAD_ACCESS in -[NSCell dealloc] during
   -[NSApplication _deallocHardCore:]'s pool-draining walk of every
   window/control) -- this app's entire UI is built by hand in code, no
   NIB/XIB, and Tiger's AppKit has real, documented deallocation-order
   issues with programmatically-built cells/controls that NIB-loaded ones
   don't hit (matches the visible symptom: the window does its normal
   fade-out close animation, then crashes mid-teardown). None of that
   matters here -- once AA has been launched there is nothing left this
   process needs to clean up gracefully, so skip AppKit's own shutdown
   entirely and let the kernel reclaim everything at once. */
- (void)quitImmediately
{
    exit(0);
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
