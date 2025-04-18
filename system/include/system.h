#pragma once
#include <string>
#include <vector>
#include <cstddef>

#define SYS_BEG namespace sys {
#define SYS_END }

SYS_BEG
std::string get_selected_text();
// return jpg image of focused window in bytes
std::vector<std::byte> capture_focused();
SYS_END
