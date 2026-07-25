#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface AppDelegate : NSObject
{
    NSWindow  *window;
    WebView   *webView;
    NSString  *aaBinary;
    NSString  *serverBinary;
    NSString  *port;
}

- (id)initWithAABinary:(NSString *)aAaBinary
           serverBinary:(NSString *)aServerBinary
                   port:(NSString *)aPort;

- (void)startServer:(id)sender;
- (void)playNetwork:(id)sender;
- (void)playSinglePlayer:(id)sender;

@end
