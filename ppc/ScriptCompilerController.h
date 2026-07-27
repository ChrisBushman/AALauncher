#import <Cocoa/Cocoa.h>

/* Standalone (non-modal) window wrapping AAScriptCompiler's "SC <script>
   <output>" CLI -- lets a content author compile a .SRC/.SRP without
   leaving the launcher. Shown via -showWindow, independent of the main
   launcher window (which stays usable alongside it). */
@interface ScriptCompilerController : NSObject
{
    NSWindow    *window;
    NSTextField *scriptField;
    NSTextField *outputField;
    NSButton    *compileButton;
    NSTextView  *logView;
    NSString    *compilerPath;
    NSTask      *task;
    NSFileHandle *taskOutput;
}

- (id)initWithCompilerPath:(NSString *)aCompilerPath;
- (void)showWindow;

@end
