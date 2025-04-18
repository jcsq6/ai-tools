#include "system.h"
#include <iostream>
#include <print>
#include <thread>
#include <fstream>
#include <filesystem>

#include "entry.h"

int app::run(int, char**)
{
    std::println("Getting selected text...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto selected = sys::get_selected_text();
    if (selected.empty())
        std::println("No text selected");
    else
        std::println("Selected text: {}", selected);
    
    auto focused = sys::capture_focused();
    if (focused.empty())
        std::println("No focused window");
    auto home = std::getenv("HOME");
    std::string home_dir = home ? home : ".";
    std::filesystem::path output = std::filesystem::path(home_dir) / "screenshot.jpg";
    std::ofstream file(output, std::ios::binary);
    file.write(reinterpret_cast<const char*>(focused.data()), focused.size());
    return 0;
}
