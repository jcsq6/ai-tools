#include "system.h"
#include "entry.h"

#include <print>
#include <thread>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <algorithm>

int app::run(int argc, char **argv)
{
    auto args = std::ranges::subrange(argv, argv + argc) | std::views::drop(1) | std::views::transform([](auto arg) { return std::string_view(arg); });

    std::println("Waiting for 2 seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto window = sys::window::get_focused();
    if (!window)
    {
        std::println("Failed to get focused window: {}", window.error());
        return 1;
    }

    std::println("Focused window: {}", window->get_name().value_or("Unknown"));
    
    std::println("Waiting for 2 seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (std::ranges::contains(args, "--refocus"))
    {
        if (auto res = window->focus())
            std::println("Refocused window");
        else
            std::println("Failed to refocus window: {}", res.error());
    }

    if (std::ranges::contains(args, "--selected"))
    {
        if (auto selected = window->get_selected())
            std::println("Selected text: {}", *selected);
        else
            std::println("Failed to get selected text: {}", selected.error());
    }
    
    if (std::ranges::contains(args, "--screenshot"))
    {
        if (auto image = window->get_screenshot())
        {
            auto home = std::getenv("HOME");
            std::string home_dir = home ? home : ".";
            std::filesystem::path output = std::filesystem::path(home_dir) / "screenshot.jpg";
            std::ofstream file(output, std::ios::binary);
            file.write(reinterpret_cast<const char*>(image->data()), image->size());

            std::println("Screenshot saved to: {}", output.string());
        }
        else
            std::println("Failed to get screenshot: {}", image.error());
    }

    if (std::ranges::contains(args, "--paste"))
    {
        if (auto res = sys::paste("testing!!"))
            std::println("Pasted text successfully");
        else
            std::println("Failed to paste text: {}", res.error());
    }

    return 0;
}
