#pragma once
#include "system.h"

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

    std::string get_selected_text();
    std::string ocr(const CFptr<CGImageRef> &image);
    CFptr<CGImageRef> get_focused_window();
    void open_image(const CFptr<CGImageRef> &image);
    
    CFptr<CGImageRef> screenshot(const CGRect region);
};

SYS_END
