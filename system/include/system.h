#pragma once
#include <cmath>
#include <string>
#include <vector>
#include <cstddef>
#include <expected>

#define SYS_BEG namespace sys {
#define SYS_END }

SYS_BEG
std::expected<std::string, std::string> get_selected_text();
// return jpg image of focused window in bytes
std::expected<std::vector<std::byte>, std::string> capture_focused();
// return jpg image of screen in bytes
std::expected<std::vector<std::byte>, std::string> capture_screen();

// copy text to clipboard
std::expected<void, std::string> copy(std::string_view text);

// "paste" text
std::expected<void, std::string> paste(std::string_view text);
SYS_END
