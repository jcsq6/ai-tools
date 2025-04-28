#import <ApplicationServices/ApplicationServices.h>
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

//template <typename T>
//CFptr<T> get_parameterized_element(CFStringRef attribute, AXUIElementRef element, CFTypeRef parameter)
//{
//  CFTypeRef value = nullptr;
//  AXError err = AXUIElementCopyParameterizedAttributeValue(
//                  element,
//                  attribute,
//                  parameter,
//                  &value
//                );
//  if (err != kAXErrorSuccess || !value)
//    throw std::runtime_error(std::format(
//      "AXUIElementCopyParameterizedAttributeValue failed: {}",
//      (int)err
//    ));
//  return CFptr<T>((T)value);
//}

template <typename T = AXUIElementRef>
CFptr<T> get_element(CFStringRef element, AXUIElementRef parent = NULL)
{
    bool free = false;
    if (!parent)
    {
        if (!(parent = AXUIElementCreateSystemWide()))
            throw std::runtime_error("Failed to create system-wide element");
        free = true;
    }
    
    T focused = NULL;
    AXError errFocus = AXUIElementCopyAttributeValue(parent,
                                                     element,
                                                     (CFTypeRef *)&focused);
    if (free)
        CFRelease(parent);
    
    if (errFocus != kAXErrorSuccess || !focused)
        throw std::runtime_error(std::format("Failed to get element: {}", (int)errFocus));
    
    return CFptr<T>(focused);
}

typedef NSArray<NSDictionary<NSPasteboardType, NSData *> *> PBBackup;

static PBBackup *snapshot_pasteboard(NSPasteboard *pb)
{
    NSMutableArray *backup = [NSMutableArray arrayWithCapacity:pb.pasteboardItems.count];

    for (NSPasteboardItem *item in pb.pasteboardItems) {
        NSMutableDictionary *payload = [NSMutableDictionary dictionaryWithCapacity:item.types.count];

        for (NSPasteboardType type in item.types) {
            NSData *data = [item dataForType:type];
            if (data) payload[type] = data;
        }
        [backup addObject:payload];
    }
    return backup;
}

static void restore_pasteboard(NSPasteboard *pb, PBBackup *backup)
{
    [pb clearContents];

    NSMutableArray<NSPasteboardItem *> *items = [NSMutableArray arrayWithCapacity:backup.count];
    for (NSDictionary<NSPasteboardType, NSData *> *payload in backup) {
        NSPasteboardItem *item = [[NSPasteboardItem alloc] init];
        for (NSPasteboardType type in payload) {
            [item setData:payload[type] forType:type];
        }
        [items addObject:item];
    }
    [pb writeObjects:items];
}


std::string Manager::get_selected_text()
{
    try
    {
        auto focused = get_element(kAXFocusedUIElementAttribute);
        
        CFArrayRef names = NULL;
        AXError err = AXUIElementCopyAttributeNames(focused.get(), &names);
        if (err == kAXErrorSuccess && names != NULL)
        {
            NSArray *attributeList = CFBridgingRelease(names);

            if ([attributeList containsObject:(__bridge NSString *)kAXSelectedTextAttribute])
            {
                auto selected_text = get_element(kAXSelectedTextAttribute, focused.get());
                NSString *text = [(__bridge NSString *)selected_text.get() copy];
                return std::string([text UTF8String]);
            }
        }
    }
    catch (const std::runtime_error &e)
    {
    }
    
    auto pb = [NSPasteboard generalPasteboard];
    PBBackup *backup = snapshot_pasteboard(pb);
    [pb clearContents];
    
    auto src = CFptr(CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
    auto down = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_C, true));
    auto up = CFptr(CGEventCreateKeyboardEvent(src.get(), (CGKeyCode)kVK_ANSI_C, false));
    CGEventSetFlags(down.get(), kCGEventFlagMaskCommand);
    CGEventSetFlags(up.get(), kCGEventFlagMaskCommand);
    CGEventPost(kCGSessionEventTap, down.get());
    CGEventPost(kCGSessionEventTap, up.get());
    
    [NSThread sleepForTimeInterval:.05];
    
    NSString *copied = [pb stringForType:NSPasteboardTypeString];
    
    restore_pasteboard(pb, backup);
    
    if (!copied)
        throw std::runtime_error("Failed to copy text");
    
    return std::string([copied UTF8String]);
}

std::string Manager::ocr(const CFptr<CGImageRef> &image)
{
    open_image(image);
    return "opened";
}

CFptr<CGImageRef> Manager::get_focused_window()
{
    CGRect frame;
    
    try
    {
            auto app = get_element(kAXFocusedApplicationAttribute);
            auto focused = get_element(kAXFocusedWindowAttribute, app.get());
            auto frame_value = get_element<AXValueRef>(CFSTR("AXFrame"), focused.get());
        
            AXValueGetValue(frame_value.get(), kAXValueTypeCGRect, &frame);
    }
    catch (...)
    {
        NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (!front)
            throw std::runtime_error("Unable to determine the frontmost application");

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
            throw std::runtime_error("No windows found");

        NSDictionary *bounds = info[(id)kCGWindowBounds];
        if (!CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)bounds, &frame))
            throw std::runtime_error("Failed to get window bounds");
    }
    
    auto image = screenshot(frame);
    if (!image)
        throw std::runtime_error("Couldn't take screenshot");
    return image;
}

CFptr<CGImageRef> Manager::screenshot(const CGRect region)
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
        throw std::runtime_error(std::format("Failed to capture image: {}", [err.localizedDescription UTF8String]));
    
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

std::string get_selected_text(void)
{
    try
    {
        return Manager::get_instance().get_selected_text();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return {};
    }
}

std::vector<std::byte> capture_focused()
{
    auto image = Manager::get_instance().get_focused_window();
    
    auto data = CFptr(CFDataCreateMutable(NULL, 0));
    if (!data)
        throw std::runtime_error("Failed to create data");
    
    CFStringRef type = (__bridge CFStringRef)[[UTTypePNG identifier] copy];
    auto dest = CFptr(CGImageDestinationCreateWithData(data.get(), type, 1, NULL));
    if (!dest)
        throw std::runtime_error("Failed to create image destination");
    
    CGImageDestinationAddImage(dest.get(), image.get(), NULL);
    if (!CGImageDestinationFinalize(dest.get()))
        throw std::runtime_error("Failed to finalize image destination");
    
    const UInt8 *bytes = CFDataGetBytePtr(data.get());
    CFIndex length = CFDataGetLength(data.get());
    
    std::vector<std::byte> result(reinterpret_cast<const std::byte *>(bytes), reinterpret_cast<const std::byte *>(bytes) + length);
    return result;
}

std::vector<std::byte> capture_screen()
{
    return {};
}

void copy(std::string_view text)
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    
    NSString *str = [NSString stringWithUTF8String:text.data()];
    if (!str)
        throw std::runtime_error("Failed to create NSString");
    
    [pb setString:str forType:NSPasteboardTypeString];
}

SYS_END
