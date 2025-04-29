#import <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreGraphics/CGEventTypes.h>
#include <Foundation/NSObjCRuntime.h>
#include <CoreGraphics/CGImage.h>
#include <__expected/unexpect.h>
#include <__expected/expected.h>
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

Manager::Manager()
{
    if (!request_screen() || !request_accessibility())
        throw std::runtime_error("Failed to request permissions");
}

template <typename T>
std::expected<CFptr<T>, std::string> get_parameterized_element(CFStringRef attribute, AXUIElementRef element, CFTypeRef parameter)
{
 CFTypeRef value = nullptr;
 AXError err = AXUIElementCopyParameterizedAttributeValue(
                 element,
                 attribute,
                 parameter,
                 &value
               );
 if (err != kAXErrorSuccess || !value)
   return std::unexpected(std::format("AXUIElementCopyParameterizedAttributeValue failed: {}", (int)err));
 return CFptr<T>((T)value);
}

template <typename T = AXUIElementRef>
std::expected<CFptr<T>, std::string> get_element(CFStringRef element, AXUIElementRef parent = NULL)
{
    bool free = false;
    if (!parent)
    {
        if (!(parent = AXUIElementCreateSystemWide()))
            return std::unexpected("Failed to create system-wide element");
        free = true;
    }
    
    T focused = NULL;
    AXError errFocus = AXUIElementCopyAttributeValue(parent,
                                                     element,
                                                     (CFTypeRef *)&focused);
    if (free)
        CFRelease(parent);
    
    if (errFocus != kAXErrorSuccess || !focused)
        return std::unexpected(std::format("Failed to get element: {}", (int)errFocus));
    
    return CFptr<T>(focused);
}

static std::expected<std::string, std::string> try_ax_selected_text() {
    auto focused = get_element(kAXFocusedUIElementAttribute);
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

auto backup_clipboard(NSPasteboard *pb)
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

void restore_clipboard(NSPasteboard *pb, NSArray<NSPasteboardItem *> *backup)
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [pb clearContents];
        [pb writeObjects:backup];
    });
}

static std::expected<std::string, std::string> try_clipboard()
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

std::expected<std::string, std::string> Manager::get_selected_text()
{
    if (auto ax = try_ax_selected_text())
        return *ax;
    else
        std::print(std::cerr, "AX error: {}\n", ax.error());

    if (auto clipboard = try_clipboard())
        return *clipboard;
    else
        std::print(std::cerr, "Clipboard error: {}\n", clipboard.error());
    
    return std::unexpected("No text found");
}

std::expected<CFptr<CGImageRef>, std::string> Manager::get_focused_window()
{
    CGRect frame;
    if (auto app = get_element(kAXFocusedApplicationAttribute))
    {   
        if (auto focused = get_element(kAXFocusedWindowAttribute, app->get()))
            if (auto frame_value = get_element<AXValueRef>(CFSTR("AXFrame"), focused->get()))
                AXValueGetValue(frame_value->get(), kAXValueTypeCGRect, &frame);
            else
                return std::unexpected("Failed to get focused window");
        else
            return std::unexpected("No focused window");
    }
    else
    {
        NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (!front)
            return std::unexpected("No frontmost application");

        pid_t owner = [front processIdentifier];

        CFArrayRef window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        NSArray *windows = CFBridgingRelease(window_list);
        NSDictionary *info = NULL;
        for (NSDictionary *win in windows)
            if ([win[(id)kCGWindowLayer] intValue] == 0 && [win[(id)kCGWindowOwnerPID] intValue] == owner)
            {
                info = win;
                break;
            }
        
        if (!info)
            return std::unexpected("No windows found");

        NSDictionary *bounds = info[(id)kCGWindowBounds];
        if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)bounds, &frame))
            return std::unexpected("Failed to get window bounds");
    }
    
    if (auto image = screenshot(frame))
        return image;
    else
        return std::unexpected(std::format("Failed to capture image: {}", image.error()));
}

std::expected<CFptr<CGImageRef>, std::string> Manager::screenshot(const CGRect region)
{
    __block CGImageRef captured = NULL;
    __block NSError *err = nil;
    
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [SCScreenshotManager captureImageInRect:region completionHandler:^(CGImageRef  _Nullable image, NSError * _Nullable error) {
        if (error)
            err = error;
        else
            captured = CGImageRetain(image);
        dispatch_semaphore_signal(sem);
    }];
    
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    
    if (err)
        return std::unexpected(std::format("Failed to capture image: {}", [err.localizedDescription UTF8String]));
    
    return CFptr(captured);
}

void Manager::open_image(const CFptr<CGImageRef> &image)
{
    NSString *tmpDirectory = NSTemporaryDirectory();
    NSString *output = [tmpDirectory stringByAppendingPathComponent:@"out.png"];
    
    CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:output];
    auto dest = CFptr(CGImageDestinationCreateWithURL(url, (__bridge  CFStringRef)[UTTypePNG identifier], 1, NULL));
    CGImageDestinationAddImage(dest.get(), image.get(), NULL);
    if (!CGImageDestinationFinalize(dest.get()))
        throw std::runtime_error("Failed to finalize image destination");
    
    NSURL *imageURL = [NSURL fileURLWithPath:output];
    if (![[NSWorkspace sharedWorkspace] openURL:imageURL])
        throw std::runtime_error("Failed to open image");
    
    [NSThread sleepForTimeInterval:.5];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *deleteError = nil;
    if (![fileManager removeItemAtPath:output error:&deleteError])
        throw std::runtime_error("Failed to delete image");
}

std::expected<std::string, std::string> get_selected_text(void)
{
    return Manager::get_instance().get_selected_text();
}

std::expected<std::vector<std::byte>, std::string> capture_focused()
{
    auto image = Manager::get_instance().get_focused_window();
    if (!image)
        return std::unexpected("Failed to capture focused window");
    
    auto data = CFptr(CFDataCreateMutable(NULL, 0));
    if (!data)
        return std::unexpected("Failed to create data");
    
    CFStringRef type = (__bridge CFStringRef)[[UTTypePNG identifier] copy];
    auto dest = CFptr(CGImageDestinationCreateWithData(data.get(), type, 1, NULL));
    if (!dest)
        return std::unexpected("Failed to create image destination");
    
    CGImageDestinationAddImage(dest.get(), image->get(), NULL);
    if (!CGImageDestinationFinalize(dest.get()))
        return std::unexpected("Failed to finalize image destination");
    
    const UInt8 *bytes = CFDataGetBytePtr(data.get());
    CFIndex length = CFDataGetLength(data.get());
    
    std::vector<std::byte> result(reinterpret_cast<const std::byte *>(bytes), reinterpret_cast<const std::byte *>(bytes) + length);
    return result;
}

std::expected<std::vector<std::byte>, std::string> capture_screen()
{
    return std::unexpected("Capture screen not implemented");
}

std::expected<void, std::string> copy(std::string_view text)
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    
    NSString *str = [NSString stringWithUTF8String:text.data()];
    
    [pb setString:str forType:NSPasteboardTypeString];

    if (pb.changeCount == 0)
        return std::unexpected("Failed to copy to clipboard");
    return {};
}

std::expected<void, std::string> paste(std::string_view text)
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

SYS_END
