#import <Cocoa/Cocoa.h>
#include <vector>
#include "entry.h"

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Forward to the shared C++ entry point
    // int result = app::run(NSProcessInfo.processInfo.arguments.count,
    //                       (const char**)NSProcessInfo.processInfo.arguments.UTF8String);

    NSArray<NSString *> *args = NSProcessInfo.processInfo.arguments;
    std::vector<std::string> str_args;
    std::vector<char *> argv;
    for (NSString *arg in args) {
        str_args.push_back([arg UTF8String]);
        argv.push_back(str_args.back().data());
    }

    int result = app::run(static_cast<int>(argv.size()), argv.data());
    [NSApp terminate:nil];
}
@end

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        AppDelegate* delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        return NSApplicationMain(argc, argv);
    }
}
