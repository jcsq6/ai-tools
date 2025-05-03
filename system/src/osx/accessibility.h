#pragma once
#include "system.h"
#include <CoreGraphics/CGImage.h>
#include <Foundation/Foundation.h>
#include <string>
#include <iostream>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif

SYS_BEG

static auto del_ref = [](CFTypeRef r)
{
    if (r)
        CFRelease(r);
};

template <typename Ref>
class CFptr : public std::unique_ptr<std::remove_pointer_t<Ref>, decltype(del_ref)>
{
public:
    using std::unique_ptr<std::remove_pointer_t<Ref>, decltype(del_ref)>::unique_ptr;
    CFptr(Ref r) : std::unique_ptr<std::remove_pointer_t<Ref>, decltype(del_ref)>(r, del_ref) {}
};

class handle
{
public:
    handle() : ax_window(), app{} {}
    
    result<void> get_focused_ax();
    result<void> get_focused_app();
    result<std::string> get_selected();
    result<std::vector<std::byte>> get_screenshot();
    result<void> refocus();
    result<std::string> get_name();
    result<NSDictionary*> get_info();

    CFptr<AXUIElementRef> ax_window;
    NSRunningApplication *app;
};

SYS_END
