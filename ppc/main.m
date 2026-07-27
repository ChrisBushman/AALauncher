#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

static NSString *ArgValue(NSArray *args, NSString *flag)
{
    unsigned int i, count = [args count];
    for (i = 0; i < count; i++)  {
        if ([[args objectAtIndex:i] isEqualToString:flag] && (i + 1) < count)
            return [args objectAtIndex:(i + 1)];
    }
    return nil;
}

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSMutableArray *args = [NSMutableArray array];
    int i;
    for (i = 1; i < argc; i++)
        [args addObject:[NSString stringWithUTF8String:argv[i]]];

    NSString *aaPath = ArgValue(args, @"--aa-path");
    NSString *serverPath = ArgValue(args, @"--server-path");
    NSString *scriptCompilerPath = ArgValue(args, @"--script-compiler-path");
    NSString *port = ArgValue(args, @"--port");

    [NSApplication sharedApplication];

    AppDelegate *delegate = [[AppDelegate alloc] initWithAABinary:aaPath
                                                      serverBinary:serverPath
                                              scriptCompilerBinary:scriptCompilerPath
                                                              port:port];
    [NSApp setDelegate:delegate];
    [NSApp run];

    [delegate release];
    [pool release];
    return 0;
}
