#import "NetworkIPController.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>       /* struct timeval (used below); the 10.3 SDK
                               doesn't pull it in transitively the way the
                               10.4 SDK's headers do */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

/* Protocol matches AAServer's DiscoveryServerLoop (AAServer.cpp) and
   ipxserver.h's DEFAULT_DISCOVERY_PORT/DISCOVERY_REQUEST_MAGIC/
   DISCOVERY_REPLY_PREFIX -- kept as literals here rather than a shared
   header since the launcher and server are separate repos/build systems. */
#define DISCOVERY_PORT 21399
#define DISCOVERY_REQUEST_MAGIC "AASERVER_DISCOVER"
#define DISCOVERY_REPLY_PREFIX  "AASERVER_HERE:"

/* No formal NSTextFieldDelegate/NSWindowDelegate protocol declaration --
   neither exists on Tiger's SDK (delegates were informal protocols in
   that era: implement the method, no @protocol conformance needed or
   available). */
@interface NetworkIPController (Private)
- (void)onConnect:(id)sender;
- (void)onCancel:(id)sender;
- (void)onSave:(id)sender;
- (void)onTableClick:(id)sender;
- (void)onTableDoubleClick:(id)sender;
- (void)loadSavedServers;
- (NSString *)serversFilePath;
- (void)startDiscovery;
- (void)runDiscovery;
- (void)addDiscoveredServer:(NSArray *)nameHostPort;
- (void)updateConnectEnabled;
@end

@implementation NetworkIPController

- (id)init
{
    self = [super init];
    if (self)  {
        NSRect frame = NSMakeRect(0, 0, 420, 340);
        panel = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSTitledWindowMask | NSClosableWindowMask)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
        [panel setTitle:@"AALauncher Network Connection"];
        [panel setReleasedWhenClosed:NO];
        [panel setDelegate:(id)self];

        NSView *content = [panel contentView];

        NSTextField *listLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(16, 308, 388, 20)] autorelease];
        [listLabel setStringValue:@"Known servers (double-click to connect):"];
        [listLabel setEditable:NO];
        [listLabel setSelectable:NO];
        [listLabel setBezeled:NO];
        [listLabel setDrawsBackground:NO];
        [content addSubview:listLabel];

        NSScrollView *scrollView = [[[NSScrollView alloc] initWithFrame:NSMakeRect(16, 190, 388, 114)] autorelease];
        [scrollView setHasVerticalScroller:YES];
        [scrollView setBorderType:NSBezelBorder];

        serverTable = [[NSTableView alloc] initWithFrame:[[scrollView contentView] frame]];
        NSTableColumn *col = [[[NSTableColumn alloc] initWithIdentifier:@"label"] autorelease];
        [col setWidth:370];
        [[col headerCell] setStringValue:@"Server"];
        [serverTable addTableColumn:col];
        [serverTable setHeaderView:nil];
        [serverTable setDataSource:(id)self];
        [serverTable setDelegate:(id)self];
        [serverTable setTarget:self];
        [serverTable setAction:@selector(onTableClick:)];
        [serverTable setDoubleAction:@selector(onTableDoubleClick:)];
        [scrollView setDocumentView:serverTable];
        [serverTable release];
        [content addSubview:scrollView];

        serverEntries = [[NSMutableArray alloc] init];

        NSTextField *manualLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(16, 164, 388, 18)] autorelease];
        [manualLabel setStringValue:@"Or enter a host/IP and port directly:"];
        [manualLabel setEditable:NO];
        [manualLabel setSelectable:NO];
        [manualLabel setBezeled:NO];
        [manualLabel setDrawsBackground:NO];
        [content addSubview:manualLabel];

        hostField = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 138, 260, 22)];
        [hostField setDelegate:(id)self];
        [content addSubview:hostField];

        portField = [[NSTextField alloc] initWithFrame:NSMakeRect(284, 138, 70, 22)];
        [portField setStringValue:@"21300"];
        [portField setDelegate:(id)self];
        [content addSubview:portField];

        NSTextField *nameLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(16, 106, 100, 18)] autorelease];
        [nameLabel setStringValue:@"Name (to save):"];
        [nameLabel setEditable:NO];
        [nameLabel setSelectable:NO];
        [nameLabel setBezeled:NO];
        [nameLabel setDrawsBackground:NO];
        [content addSubview:nameLabel];

        nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 103, 160, 22)];
        [content addSubview:nameField];

        saveButton = [[NSButton alloc] initWithFrame:NSMakeRect(284, 102, 90, 24)];
        [saveButton setTitle:@"Save"];
        [saveButton setBezelStyle:NSRoundedBezelStyle];
        [saveButton setTarget:self];
        [saveButton setAction:@selector(onSave:)];
        [content addSubview:saveButton];

        connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(284, 16, 90, 26)];
        [connectButton setTitle:@"Connect"];
        [connectButton setBezelStyle:NSRoundedBezelStyle];
        [connectButton setTarget:self];
        [connectButton setAction:@selector(onConnect:)];
        [connectButton setEnabled:NO];
        [panel setDefaultButtonCell:[connectButton cell]];
        [content addSubview:connectButton];

        result = nil;
        resultPort = nil;
        discoverySocketShouldStop = NO;
        discoveredHostPorts = [[NSMutableSet alloc] init];

        [self loadSavedServers];
        [self startDiscovery];
    }
    return self;
}

- (void)dealloc
{
    discoverySocketShouldStop = YES;
    [hostField release];
    [portField release];
    [nameField release];
    [connectButton release];
    [saveButton release];
    [serverEntries release];
    [panel release];
    [result release];
    [resultPort release];
    [discoveredHostPorts release];
    [super dealloc];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    [self updateConnectEnabled];
}

- (void)updateConnectEnabled
{
    NSString *h = [[hostField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *p = [[portField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    [connectButton setEnabled:([h length] > 0 && [p length] > 0)];
}

- (void)onConnect:(id)sender
{
    result = [[[hostField stringValue]
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] retain];
    resultPort = [[[portField stringValue]
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

- (NSString *)serversFilePath
{
    NSString *dir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    return [dir stringByAppendingPathComponent:@"servers.txt"];
}

- (void)loadSavedServers
{
    NSString *path = [self serversFilePath];
    NSString *contents = [NSString stringWithContentsOfFile:path];
    if (contents == nil)
        return;

    NSArray *lines = [contents componentsSeparatedByString:@"\n"];
    unsigned int i, count = [lines count];
    for (i = 0; i < count; i++)  {
        NSString *line = [[lines objectAtIndex:i] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([line length] == 0 || [line hasPrefix:@"#"])
            continue;
        NSArray *parts = [line componentsSeparatedByString:@"\t"];
        if ([parts count] < 3)
            continue;
        NSDictionary *entry = [NSDictionary dictionaryWithObjectsAndKeys:
            [parts objectAtIndex:0], @"name",
            [parts objectAtIndex:1], @"host",
            [parts objectAtIndex:2], @"port",
            nil];
        [serverEntries addObject:entry];
    }
    [serverTable reloadData];
}

- (void)onSave:(id)sender
{
    NSString *host = [[hostField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *port = [[portField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([host length] == 0 || [port length] == 0)
        return;

    NSString *name = [[nameField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([name length] == 0)
        name = host;

    NSString *line = [NSString stringWithFormat:@"%@\t%@\t%@\n", name, host, port];
    NSString *path = [self serversFilePath];
    NSString *existing = [NSString stringWithContentsOfFile:path];
    NSString *combined = existing ? [existing stringByAppendingString:line] : line;
    [combined writeToFile:path atomically:YES];

    NSDictionary *entry = [NSDictionary dictionaryWithObjectsAndKeys:
        name, @"name", host, @"host", port, @"port", nil];
    [serverEntries addObject:entry];
    [serverTable reloadData];
}

- (void)selectEntryAtRow:(int)row
{
    if (row < 0 || row >= (int)[serverEntries count])
        return;
    NSDictionary *entry = [serverEntries objectAtIndex:row];
    [hostField setStringValue:[entry objectForKey:@"host"]];
    [portField setStringValue:[entry objectForKey:@"port"]];
    [self updateConnectEnabled];
}

- (void)onTableClick:(id)sender
{
    [self selectEntryAtRow:[serverTable clickedRow]];
}

- (void)onTableDoubleClick:(id)sender
{
    [self selectEntryAtRow:[serverTable clickedRow]];
    [self onConnect:sender];
}

/* NSTableViewDataSource (informal protocol on Tiger). */
- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [serverEntries count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    NSDictionary *entry = [serverEntries objectAtIndex:row];
    return [NSString stringWithFormat:@"%@  (%@:%@)", [entry objectForKey:@"name"], [entry objectForKey:@"host"], [entry objectForKey:@"port"]];
}

/* Spawns a background thread that broadcasts a discovery probe and adds
   each reply to the table as it arrives. Raw BSD sockets (not
   NSURLConnection/CFSocket) since this needs a simple broadcast+timeout
   recv loop, same style as the IRIX launcher's discovery client. Runs on
   NSThread, not the main thread, since recvfrom() blocks; results are
   marshaled back via performSelectorOnMainThread: with explicit modes so
   they're delivered even while runModalForWindow:'s modal loop (a
   different run loop mode than the default) is running. */
- (void)startDiscovery
{
    [NSThread detachNewThreadSelector:@selector(runDiscovery) toTarget:self withObject:nil];
}

- (void)runDiscovery
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int sock;
    int broadcastEnable = 1;
    struct sockaddr_in destAddr, fromAddr;
    struct timeval tv;
    const char *request = DISCOVERY_REQUEST_MAGIC;
    char buffer[128];
    NSTimeInterval deadline;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)  {
        [pool release];
        return;
    }

    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    tv.tv_sec = 0;
    tv.tv_usec = 200000;  /* 200ms recv timeout, polled until deadline below */
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(DISCOVERY_PORT);
    destAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(sock, request, strlen(request), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));

    deadline = [NSDate timeIntervalSinceReferenceDate] + 1.5;
    while ([NSDate timeIntervalSinceReferenceDate] < deadline)  {
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen);
        if (n <= 0)
            continue;  /* timeout or transient error -- keep polling until deadline */
        buffer[n] = '\0';

        if (strncmp(buffer, DISCOVERY_REPLY_PREFIX, strlen(DISCOVERY_REPLY_PREFIX)) == 0)  {
            char *rest = buffer + strlen(DISCOVERY_REPLY_PREFIX);
            char *colon = strchr(rest, ':');
            if (colon != NULL)  {
                *colon = '\0';
                NSString *port = [NSString stringWithUTF8String:rest];
                NSString *name = [NSString stringWithUTF8String:(colon + 1)];
                NSString *host = [NSString stringWithUTF8String:inet_ntoa(fromAddr.sin_addr)];
                /* Explicitly owned (alloc/init, not autoreleased) rather
                   than relying on performSelectorOnMainThread: to retain
                   it for us across the thread hop -- confirmed on real
                   Tiger hardware that it doesn't reliably: this function
                   returns and drains its own autorelease pool well before
                   the main thread's run loop gets around to invoking the
                   async (waitUntilDone:NO) call below, deallocating an
                   autoreleased array out from under it (use-after-free on
                   the main thread once it finally ran: "selector not
                   recognized" on reused memory, then double-free on
                   teardown). addDiscoveredServer: below releases this once
                   it's done with it. */
                NSArray *info = [[NSArray alloc] initWithObjects:name, host, port, nil];

                /* The 4-arg (modes:) form, not the plain 3-arg
                   performSelectorOnMainThread:withObject:waitUntilDone: --
                   this dialog runs under runModalForWindow:'s modal loop
                   (NSModalPanelRunLoopMode, not the default run loop mode),
                   so without explicitly including it a reply would only be
                   delivered after the modal loop happens to end. Available
                   since Tiger (unlike -performSelector:onThread:..., a
                   10.5+ addition, which can't be used in this PPC/Tiger
                   target). */
                [self performSelectorOnMainThread:@selector(addDiscoveredServer:)
                                        withObject:info
                                     waitUntilDone:NO
                                             modes:[NSArray arrayWithObjects:NSDefaultRunLoopMode, NSModalPanelRunLoopMode, nil]];
            }
        }
    }

    close(sock);
    [pool release];
}

- (void)addDiscoveredServer:(NSArray *)nameHostPort
{
    NSString *name = [nameHostPort objectAtIndex:0];
    NSString *host = [nameHostPort objectAtIndex:1];
    NSString *port = [nameHostPort objectAtIndex:2];

    NSString *key = [NSString stringWithFormat:@"%@:%@", host, port];
    if (![discoveredHostPorts containsObject:key])  {
        [discoveredHostPorts addObject:key];
        NSDictionary *entry = [NSDictionary dictionaryWithObjectsAndKeys:
            [name stringByAppendingString:@" [LAN]"], @"name", host, @"host", port, @"port", nil];
        [serverEntries addObject:entry];
        [serverTable reloadData];
    }
    /* Balances the explicit alloc/init in runDiscovery -- see its comment. */
    [nameHostPort release];
}

- (NSString *)runModal
{
    [panel center];
    [NSApp runModalForWindow:panel];
    [panel close];
    return [result autorelease];
}

- (NSString *)selectedPort
{
    return [resultPort autorelease];
}

@end
