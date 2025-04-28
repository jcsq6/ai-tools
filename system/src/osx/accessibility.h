#pragma once
#include "system.h"
#include <CoreGraphics/CGImage.h>
#include <__expected/expected.h>
#include <string>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif

SYS_BEG

static constexpr auto del_ref = [](CFTypeRef r)
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

class Manager 
{
    Manager();
public:
    static Manager& get_instance()
    {
        static Manager instance;
        return instance;
    }

    std::expected<std::string, std::string> get_selected_text();
    std::expected<CFptr<CGImageRef>, std::string> get_focused_window();
    void open_image(const CFptr<CGImageRef> &image);
    
    std::expected<CFptr<CGImageRef>, std::string> screenshot(const CGRect region);
};

SYS_END
