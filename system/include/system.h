#pragma once
#include <__expected/expected.h>
#include <string>
#include <vector>
#include <cstddef>
#include <expected>

#define SYS_BEG namespace sys {
#define SYS_END }

SYS_BEG

template <typename T>
using result = std::expected<T, std::string>;

class handle;

class window
{
public:
    window();
    ~window();
    
    window(const window&) = delete;
    window& operator=(const window&) = delete;

    window(window&&);
    window& operator=(window&&);

    static result<window> get_focused();

    result<std::string> get_selected() const;
    result<std::vector<std::byte>> get_screenshot() const;
    result<void> focus() const;

    result<std::string>  get_name() const;
private:
    std::unique_ptr<handle> M_handle;
};

// return jpg image of screen in bytes
result<std::vector<std::byte>> capture_screen();

// copy text to clipboard
result<void> copy(std::string_view text);
// "paste" text
result<void> paste(std::string_view text);
SYS_END
