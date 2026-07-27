#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@class ScriptCompilerController;

@interface AppDelegate : NSObject
{
    NSWindow  *window;
    WebView   *webView;
    NSTextField *portField;
    NSString  *aaBinary;
    NSString  *serverBinary;
    NSString  *scriptCompilerBinary;
    NSString  *initialPort;
    ScriptCompilerController *scriptCompilerController;
}

- (id)initWithAABinary:(NSString *)aAaBinary
           serverBinary:(NSString *)aServerBinary
   scriptCompilerBinary:(NSString *)aScriptCompilerBinary
                   port:(NSString *)aPort;

- (void)startServer:(id)sender;
- (void)playNetwork:(id)sender;
- (void)playSinglePlayer:(id)sender;
- (void)openScriptCompiler:(id)sender;
- (void)exitApplication:(id)sender;

@end
