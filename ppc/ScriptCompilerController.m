#import "ScriptCompilerController.h"

@interface ScriptCompilerController (Private)
- (void)onBrowseScript:(id)sender;
- (void)onBrowseOutput:(id)sender;
- (void)onCompile:(id)sender;
- (void)appendLog:(NSString *)text;
- (void)taskOutputAvailable:(NSNotification *)note;
- (void)taskDidTerminate:(NSNotification *)note;
@end

@implementation ScriptCompilerController

- (id)initWithCompilerPath:(NSString *)aCompilerPath
{
    self = [super init];
    if (self)  {
        compilerPath = [aCompilerPath retain];

        NSRect frame = NSMakeRect(0, 0, 600, 447);
        window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
        [window setTitle:@"Amulets & Armor Script Compiler"];
        [window setReleasedWhenClosed:NO];

        NSView *content = [window contentView];

        NSTextField *scriptLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(12, 412, 70, 18)] autorelease];
        [scriptLabel setStringValue:@"Script file:"];
        [scriptLabel setEditable:NO];
        [scriptLabel setSelectable:NO];
        [scriptLabel setBezeled:NO];
        [scriptLabel setDrawsBackground:NO];
        [content addSubview:scriptLabel];

        scriptField = [[NSTextField alloc] initWithFrame:NSMakeRect(90, 410, 400, 22)];
        [content addSubview:scriptField];

        NSButton *browseScript = [[[NSButton alloc] initWithFrame:NSMakeRect(496, 408, 90, 24)] autorelease];
        [browseScript setTitle:@"Browse..."];
        [browseScript setBezelStyle:NSRoundedBezelStyle];
        [browseScript setTarget:self];
        [browseScript setAction:@selector(onBrowseScript:)];
        [content addSubview:browseScript];

        NSTextField *outputLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(12, 382, 70, 18)] autorelease];
        [outputLabel setStringValue:@"Output file:"];
        [outputLabel setEditable:NO];
        [outputLabel setSelectable:NO];
        [outputLabel setBezeled:NO];
        [outputLabel setDrawsBackground:NO];
        [content addSubview:outputLabel];

        outputField = [[NSTextField alloc] initWithFrame:NSMakeRect(90, 380, 400, 22)];
        [content addSubview:outputField];

        NSButton *browseOutput = [[[NSButton alloc] initWithFrame:NSMakeRect(496, 378, 90, 24)] autorelease];
        [browseOutput setTitle:@"Browse..."];
        [browseOutput setBezelStyle:NSRoundedBezelStyle];
        [browseOutput setTarget:self];
        [browseOutput setAction:@selector(onBrowseOutput:)];
        [content addSubview:browseOutput];

        compileButton = [[NSButton alloc] initWithFrame:NSMakeRect(12, 342, 574, 28)];
        [compileButton setTitle:@"Compile"];
        [compileButton setBezelStyle:NSRoundedBezelStyle];
        [compileButton setTarget:self];
        [compileButton setAction:@selector(onCompile:)];
        [content addSubview:compileButton];

        NSScrollView *logScroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(12, 12, 574, 320)] autorelease];
        [logScroll setHasVerticalScroller:YES];
        [logScroll setBorderType:NSBezelBorder];
        [logScroll setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

        logView = [[NSTextView alloc] initWithFrame:[[logScroll contentView] frame]];
        [logView setEditable:NO];
        [logView setString:@"Compiler output will appear here..."];
        [logScroll setDocumentView:logView];
        [content addSubview:logScroll];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [compilerPath release];
    [scriptField release];
    [outputField release];
    [compileButton release];
    [logView release];
    [window release];
    [super dealloc];
}

- (void)showWindow
{
    [window center];
    [window makeKeyAndOrderFront:nil];
}

- (void)onBrowseScript:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    if ([panel runModalForTypes:nil] == NSOKButton)  {
        NSString *path = [panel filename];
        [scriptField setStringValue:path];
        if ([[outputField stringValue] length] == 0)  {
            NSString *base = [[path lastPathComponent] stringByDeletingPathExtension];
            NSString *dir = [path stringByDeletingLastPathComponent];
            [outputField setStringValue:[dir stringByAppendingPathComponent:[base stringByAppendingString:@".OUT"]]];
        }
    }
}

- (void)onBrowseOutput:(id)sender
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    if ([[outputField stringValue] length] > 0)
        [panel setDirectory:[[outputField stringValue] stringByDeletingLastPathComponent]];
    if ([panel runModal] == NSOKButton)  {
        [outputField setStringValue:[panel filename]];
    }
}

- (void)onCompile:(id)sender
{
    NSString *scriptPath = [[scriptField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *outputPath = [[outputField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([scriptPath length] == 0 || [outputPath length] == 0)  {
        NSRunAlertPanel(@"Script Compiler", @"Please choose both a script file and an output file.", @"OK", nil, nil);
        return;
    }
    if (![[NSFileManager defaultManager] fileExistsAtPath:compilerPath])  {
        NSRunAlertPanel(@"Script Compiler", @"Compiler not found: %@", @"OK", nil, nil, compilerPath);
        return;
    }

    [logView setString:@""];
    [compileButton setEnabled:NO];

    NSPipe *pipe = [NSPipe pipe];
    taskOutput = [pipe fileHandleForReading];

    task = [[NSTask alloc] init];
    [task setLaunchPath:compilerPath];
    [task setArguments:[NSArray arrayWithObjects:scriptPath, outputPath, nil]];
    [task setStandardOutput:pipe];
    [task setStandardError:pipe];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(taskOutputAvailable:)
                                                  name:NSFileHandleReadCompletionNotification
                                                object:taskOutput];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(taskDidTerminate:)
                                                  name:NSTaskDidTerminateNotification
                                                object:task];

    [taskOutput readInBackgroundAndNotify];

    @try  {
        [task launch];
    }
    @catch (NSException *e)  {
        NSRunAlertPanel(@"Script Compiler", @"Failed to launch compiler: %@", @"OK", nil, nil, [e reason]);
        [compileButton setEnabled:YES];
    }
}

- (void)taskOutputAvailable:(NSNotification *)note
{
    NSData *data = [[note userInfo] objectForKey:NSFileHandleNotificationDataItem];
    if ([data length] > 0)  {
        NSString *text = [[[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding] autorelease];
        [self appendLog:text];
        [taskOutput readInBackgroundAndNotify];
    }
}

- (void)taskDidTerminate:(NSNotification *)note
{
    int status = [task terminationStatus];
    [self appendLog:(status == 0 ? @"\n-- Compile succeeded. --" : @"\n-- Compile failed. --")];
    [compileButton setEnabled:YES];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSFileHandleReadCompletionNotification object:taskOutput];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSTaskDidTerminateNotification object:task];
    [task release];
    task = nil;
}

- (void)appendLog:(NSString *)text
{
    NSString *combined = [[logView string] stringByAppendingString:text];
    [logView setString:combined];
    [logView scrollRangeToVisible:NSMakeRange([combined length], 0)];
}

@end
