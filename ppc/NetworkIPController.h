#import <Cocoa/Cocoa.h>

/* "Join Network Game" dialog -- run via -runModal, which returns the
   entered/selected host (trimmed), or nil if the user cancelled; the
   matching port is then available via -selectedPort. Lists saved
   favorites (servers.txt alongside the launcher binary) and live
   LAN-discovered servers (see startDiscovery), or a host/IP + port can be
   typed directly. Matches mac/NetworkIPDialog.cpp's behavior. */
@interface NetworkIPController : NSObject
{
    NSWindow     *panel;
    NSTableView  *serverTable;
    NSMutableArray *serverEntries;  // array of NSDictionary{name,host,port}
    NSTextField  *hostField;
    NSTextField  *portField;
    NSTextField  *nameField;
    NSButton     *connectButton;
    NSButton     *saveButton;
    NSString     *result;
    NSString     *resultPort;
    BOOL          discoverySocketShouldStop;
}

- (NSString *)runModal;
- (NSString *)selectedPort;

@end
