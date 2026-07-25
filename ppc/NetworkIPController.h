#import <Cocoa/Cocoa.h>

/* Modal "enter server IP" dialog -- run via -runModal, which returns the
   entered IP address (trimmed), or nil if the user cancelled. Matches
   mac/NetworkIPDialog.cpp's behavior (Connect disabled until non-empty). */
@interface NetworkIPController : NSObject
{
    NSWindow    *panel;
    NSTextField *ipField;
    NSButton    *connectButton;
    NSString    *result;
}

- (NSString *)runModal;

@end
