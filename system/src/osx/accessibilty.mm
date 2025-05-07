#import <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreGraphics/CGEventTypes.h>
#include <Foundation/NSObjCRuntime.h>
#include <CoreGraphics/CGImage.h>
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <Vision/Vision.h>

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <AVFoundation/AVFoundation.h>

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>
#include <iostream>
#include <format>
#include <memory>
#include <type_traits>

#include "accessibility.h"
#include "system.h"

SYS_BEG

BOOL request_accessibility()
{
    if (!AXIsProcessTrusted())
    {
        const void *keys[] = { kAXTrustedCheckOptionPrompt };
        const void *values[] = { kCFBooleanTrue };
        auto options = CFptr(CFDictionaryCreate(NULL,
                                                keys,
                                                values,
                                                1,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks));
        if (!AXIsProcessTrustedWithOptions(options.get()))
        {
            NSLog(@"Accessibility privileges denied.");
            return NO;
        }
    }
    
    return YES;
}

BOOL request_screen()
{
    if (!CGPreflightScreenCaptureAccess())
        // Prompt the user for access.
        if (!CGRequestScreenCaptureAccess()) {
            NSLog(@"Screen recording privileges denied.");
            return NO;
        }
    return YES;
}

template <typename T>
result<CFptr<T>> get_parameterized_element(CFStringRef attribute, AXUIElementRef element, CFTypeRef parameter)
{
 CFTypeRef value = nullptr;
 AXError err = AXUIElementCopyParameterizedAttributeValue(
                 element,
                 attribute,
                 parameter,
                 &value
               );
 if (err != kAXErrorSuccess || !value)
   return std::unexpected(std::format("AXUIElementCopyParameterizedAttributeValue failed ({})", (int)err));
 return CFptr<T>((T)value);
}

template <typename T = AXUIElementRef>
result<CFptr<T>> get_element(CFStringRef element, const AXUIElementRef parent = NULL)
{
    auto p = parent;
    bool free = false;
    if (!p)
    {
        if (!(p = AXUIElementCreateSystemWide()))
            return std::unexpected("Failed to create system-wide element");
        free = true;
    }
    
    T focused = NULL;
    AXError errFocus = AXUIElementCopyAttributeValue(p,
                                                     element,
                                                     (CFTypeRef *)&focused);
    if (free)
        CFRelease(p);
    
    if (errFocus != kAXErrorSuccess || !focused)
        return std::unexpected(std::format("Failed to get element ({})", (int)errFocus));
    
    return CFptr<T>(focused);
}

class Manager 
{
    Manager()
    {
        if (!request_screen() || !request_accessibility())
            throw std::runtime_error("Failed to request permissions");
    }

public:
    static Manager& get_instance()
    {
        static Manager instance;
        return instance;
    }

    result<std::string> try_ax_selected_text(const AXUIElementRef parent = NULL) {
        auto focused = get_element(kAXFocusedUIElementAttribute, parent);
        if (!focused)
            return std::unexpected("No focused element");

        CFArrayRef names = NULL;
        AXError err = AXUIElementCopyAttributeNames(focused->get(), &names);
        if (err == kAXErrorSuccess && names != NULL)
        {
            NSArray *attributeList = CFBridgingRelease(names);

            auto has_element = [attributeList](auto attr)
            {
                return [attributeList containsObject:(__bridge NSString *)attr];
            };

            if (has_element(kAXSelectedTextAttribute))
            {
                if (auto selected_text = get_element(kAXSelectedTextAttribute, focused->get()))
                {
                    NSString *text = [(__bridge NSString *)selected_text->get() copy];
                    return std::string([text UTF8String]);
                }
            }
            if (has_element(kAXSelectedTextRangeAttribute))
            {
                auto range_value = get_element<AXValueRef>(kAXSelectedTextRangeAttribute, focused->get());
                if (!range_value)
                    return std::unexpected("Failed to get selected text range");
            
                CFRange range;
                AXValueGetValue(range_value->get(), kAXValueTypeCFRange, &range);

                if (auto str = get_parameterized_element<CFTypeRef>(kAXAttributedStringForRangeParameterizedAttribute, focused->get(), range_value->get()))
                {
                    NSString *text = [(__bridge NSAttributedString *)str->get() string];
                    return std::string([text UTF8String]);
                }
                else
                    return std::unexpected("Failed to get attributed string");
            }
        }

        return std::unexpected("No AX elements exposed");
    }

    static auto backup_clipboard(NSPasteboard *pb)
    {
        NSMutableArray<NSPasteboardItem *> *backup = @[].mutableCopy;
        for (NSPasteboardItem *item in [pb pasteboardItems])
        {
            NSPasteboardItem *copy = [NSPasteboardItem new];
            for (NSString *type in [item types])
            {
                NSData *data = [item dataForType:type];
                if (data)
                    [copy setData:data forType:type];
            }
            [backup addObject:copy];
        }
        return backup;
    }

    static void restore_clipboard(NSPasteboard *pb, NSArray<NSPasteboardItem *> *backup)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [pb clearContents];
            [pb writeObjects:backup];
        });
    }

    result<std::string> try_clipboard()
    {
        try
        {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            auto before = pb.changeCount;

            auto backup = backup_clipboard(pb);
            
            auto src = CFptr(CGEventSourceCreate(kCGEventSourceStateCombinedSessionState));
            auto down = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_C, true));
            auto up = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_C, false));
            CGEventSetFlags(down.get(), kCGEventFlagMaskCommand);
            CGEventSetFlags(up.get(), kCGEventFlagMaskCommand);
            CGEventPost(kCGSessionEventTap, down.get());
            CGEventPost(kCGSessionEventTap, up.get());

            const NSTimeInterval timeout = 0.5;
            NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
            while (pb.changeCount == before && [deadline timeIntervalSinceNow] > 0) {
                [NSThread sleepForTimeInterval:.01];
            }

            NSString *copied = [pb stringForType:NSPasteboardTypeString];
            
            restore_clipboard(pb, backup);

            if (!copied || [copied length] == 0)
                return std::unexpected("No text found in clipboard");
            
            return std::string([copied UTF8String]);
        }
        catch (...)
        {
            return std::unexpected(std::string("unknown error"));
        }
    }

    result<CFptr<CGImageRef>> screenshot(const CGRect region)
    {
        __block CGImageRef captured = NULL;
        __block NSError *err = nil;
        
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        if (@available(macOS 15.2, *)) {
            [SCScreenshotManager captureImageInRect:region completionHandler:^(CGImageRef  _Nullable image, NSError * _Nullable error) {
                if (error)
                    err = error;
                else
                    captured = CGImageRetain(image);
                dispatch_semaphore_signal(sem);
            }];
        } else {
            err = [NSError errorWithDomain:@"ScreenCaptureKit"
                                      code:-1
                                  userInfo:@{NSLocalizedDescriptionKey: @"ScreenCaptureKit requires macOS 15.2+"}];
            dispatch_semaphore_signal(sem);
            return std::unexpected(std::format("ScreenCaptureKit requires macOS 15.2+ ({})", [err.localizedDescription UTF8String]));
        }
        
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        
        if (err)
            return std::unexpected(std::format("Failed to capture image ({})", [err.localizedDescription UTF8String]));
        
        return CFptr(captured);
    }

    result<void> open_image(const CFptr<CGImageRef> &image)
    {
        NSString *tmpDirectory = NSTemporaryDirectory();
        NSString *output = [tmpDirectory stringByAppendingPathComponent:@"out.png"];
        
        CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:output];
        auto dest = CFptr(CGImageDestinationCreateWithURL(url, (__bridge  CFStringRef)[UTTypePNG identifier], 1, NULL));
        CGImageDestinationAddImage(dest.get(), image.get(), NULL);
        if (!CGImageDestinationFinalize(dest.get()))
            return std::unexpected("Failed to finalize image destination");
        
        NSURL *imageURL = [NSURL fileURLWithPath:output];
        if (![[NSWorkspace sharedWorkspace] openURL:imageURL])
            return std::unexpected("Failed to open image");
        
        [NSThread sleepForTimeInterval:.5];

        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSError *deleteError = nil;
        if (![fileManager removeItemAtPath:output error:&deleteError])
            return std::unexpected(std::format("Failed to delete image ({})", [[deleteError localizedDescription] UTF8String]));
        return {};
    }

    result<std::vector<std::byte>> to_jpg(const CFptr<CGImageRef> &image)
    {
        auto data = CFptr(CFDataCreateMutable(NULL, 0));
        if (!data)
            return std::unexpected("Failed to create data");
        
        CFStringRef type = (__bridge CFStringRef)[[UTTypePNG identifier] copy];
        auto dest = CFptr(CGImageDestinationCreateWithData(data.get(), type, 1, NULL));
        if (!dest)
            return std::unexpected("Failed to create image destination");
        
        CGImageDestinationAddImage(dest.get(), image.get(), NULL);
        if (!CGImageDestinationFinalize(dest.get()))
            return std::unexpected("Failed to finalize image destination");
        
        const UInt8 *bytes = CFDataGetBytePtr(data.get());
        CFIndex length = CFDataGetLength(data.get());
        
        std::vector<std::byte> result(reinterpret_cast<const std::byte *>(bytes), reinterpret_cast<const std::byte *>(bytes) + length);
        return result;
    }

    result<void> paste(std::string_view text)
    {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
    
        auto backup = backup_clipboard(pb);
        [pb clearContents];

        NSString *str = [NSString stringWithUTF8String:text.data()];
        [pb setString:str forType:NSPasteboardTypeString];

        auto src = CFptr(CGEventSourceCreate(kCGEventSourceStateCombinedSessionState));
        auto down = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_V, true));
        auto up = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_V, false));
        CGEventSetFlags(down.get(), kCGEventFlagMaskCommand);
        CGEventSetFlags(up.get(), kCGEventFlagMaskCommand);
        CGEventPost(kCGSessionEventTap, down.get());
        CGEventPost(kCGSessionEventTap, up.get());


        restore_clipboard(pb, backup);

        return {};
    }

    result<void> copy(std::string_view text)
    {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        
        NSString *str = [NSString stringWithUTF8String:text.data()];
        
        [pb setString:str forType:NSPasteboardTypeString];

        if (pb.changeCount == 0)
            return std::unexpected("Failed to copy to clipboard");
        return {};
    }
};

result<void> handle::get_focused_ax()
{
    std::string error;
    if (auto app = get_element(kAXFocusedApplicationAttribute))
    {
        if (auto focused = get_element(kAXFocusedWindowAttribute, app->get()))
        {
            ax_window = std::move(*focused);
            return {};
        }
        else
            error = std::move(focused).error();
    }
    else
        error = std::move(app).error();
    
    if (app)
    {
        auto elem = CFptr(AXUIElementCreateApplication([app processIdentifier]));
        if (!elem)
            return std::unexpected(std::format("Failed to create application element ({})", error));

        if (auto err = AXUIElementSetAttributeValue(elem.get(), CFSTR("AXManualAccessibility"), kCFBooleanTrue); err != kAXErrorSuccess)
            return std::unexpected(std::format("Failed to set manual accessibility ({}) -> {}", (int)err, error));
        std::println("Activated manual accessibility");
    }

    return {};
}


result<void> handle::get_focused_app()
{
    app = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (!app)
        return std::unexpected("No frontmost application");

    return {};
}

result<NSDictionary *> handle::get_info()
{
    pid_t owner = [app processIdentifier];

    CFArrayRef window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    NSArray *windows = CFBridgingRelease(window_list);
    for (NSDictionary *win in windows)
        if ([win[(id)kCGWindowLayer] intValue] == 0 && [win[(id)kCGWindowOwnerPID] intValue] == owner)
            return win;
    
    return std::unexpected("No windows found");
}

result<void> handle::refocus()
{
    pid_t pid = 0;
    if (ax_window)
    {
        AXUIElementGetPid(ax_window.get(), &pid);
        app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    }

    if (!app)
        return std::unexpected("No focused application");

    if (![app activateWithOptions:(NSApplicationActivateAllWindows)])
        return std::unexpected(std::format("Failed to activate application '{}', Policy = {}", [app.bundleIdentifier UTF8String], (long)app.activationPolicy));

    if (ax_window)
    {
        AXUIElementSetAttributeValue(ax_window.get(), kAXMinimizedAttribute, kCFBooleanFalse);

        if (auto err = AXUIElementPerformAction(ax_window.get(), kAXRaiseAction); err != kAXErrorSuccess)
            return std::unexpected(std::format("Failed to raise window ({})", (int)err));

        auto elem = CFptr(AXUIElementCreateApplication(pid));

        if (auto err = AXUIElementSetAttributeValue(elem.get(), kAXMainAttribute, kCFBooleanTrue); err != kAXErrorSuccess)
            std::print(std::cerr, "Failed to set main attribute ({})\n", (int)err);

        if (auto err = AXUIElementSetAttributeValue(elem.get(), kAXFocusedWindowAttribute, ax_window.get()); err != kAXErrorSuccess)
            std::print(std::cerr, "Failed to set focused window attribute ({})\n", (int)err);
    }
    
    return {};
}

result<std::string> handle::get_selected()
{
    if (ax_window)
    {
        if (auto res = Manager::get_instance().try_ax_selected_text(ax_window.get()))
            return *res;
        else
            std::print(std::cerr, "Couldn't get selected text from AX: {}\n", res.error());
    }
    
    if (auto res = refocus(); !res)
        return std::unexpected(res.error());

    if (auto res = Manager::get_instance().try_clipboard())
        return *res;
    else
        std::print(std::cerr, "Couldn't get selected text from clipboard: {}\n", res.error());

    return std::unexpected("No text found");
}

result<std::vector<std::byte>> handle::get_screenshot()
{
    if (!refocus())
        return std::unexpected("Failed to refocus window");

    CGRect frame;
    if (ax_window)
    {
        if (auto frame_value = get_element<AXValueRef>(CFSTR("AXFrame"), ax_window.get()))
            AXValueGetValue(frame_value->get(), kAXValueTypeCGRect, &frame);
        else
        {
            std::print(std::cerr, "Couldn't get frame from AX: {}\n", frame_value.error());

            if (auto info = get_info())
            {
                NSDictionary *bounds = (*info)[(id)kCGWindowBounds];
                if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)bounds, &frame))
                {
                    std::print(std::cerr, "Couldn't get frame from CG");
                    return std::unexpected("Failed to get window bounds");
                }
            }
            else
                return std::unexpected("Failed to get window bounds");
        }
    }

    auto image = Manager::get_instance().screenshot(frame);
    if (!image)
        return std::unexpected(std::format("Failed to capture image ({})", image.error()));

    return Manager::get_instance().to_jpg(*image);
}

result<std::string> handle::get_name()
{
    if (app)
    {
        NSString *name = [app localizedName];
        if (name)
            return std::string([name UTF8String]);
        else
            return std::unexpected("Failed to get application name");
    }
    else
        return std::unexpected("No focused application");
}

result<std::vector<std::byte>> capture_screen()
{
    NSScreen *screen = [NSScreen mainScreen];
    if (!screen)
        return std::unexpected("No main screen found");
    
    CGRect screenRect = [screen frame];
    auto image = Manager::get_instance().screenshot(screenRect);
    if (!image)
        return std::unexpected(std::format("Failed to capture image ({})", image.error()));
    return Manager::get_instance().to_jpg(*image);
}

result<void> copy(std::string_view text)
{
    return Manager::get_instance().copy(text);
}

result<void> paste(std::string_view text)
{
    return Manager::get_instance().paste(text);
}

window::window() : M_handle() 
{
}

result<std::string> window::get_selected() const { return M_handle->get_selected(); }
result<std::vector<std::byte>> window::get_screenshot() const { return M_handle->get_screenshot(); }
result<void> window::focus() const { return M_handle->refocus(); }
result<std::string> window::get_name() const { return M_handle->get_name(); }

window::~window() = default;

result<window> window::get_focused()
{
    auto win = window();
    win.M_handle = std::make_unique<handle>();
    if (auto err = win.M_handle->get_focused_app(); !err)
    {
        std::println(std::cerr, "Error getting focused app: {}", err.error());
        return std::unexpected(err.error());
    }

    if (auto err = win.M_handle->get_focused_ax(); !err)
        std::println(std::cerr, "Error getting focused window: {}", err.error());

    return win;
}

window::window(window&&) = default;
window& window::operator=(window&&) = default;

SYS_END
