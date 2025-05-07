#include "hotkey_handler.h"
#include "uitools.h"

#include <fstream>
#include <print>
#include <iostream>
#include <ranges>

#include <json.hpp>

#include <string>
#include <system.h>

#include <QHotkey>

hotkey_handler::hotkey::hotkey(std::string_view name, std::string_view combination, auto &&callback)
    : name(name), combination(combination), handle{}
{
    if (combination.empty())
        return;

    handle = new QHotkey(QKeySequence(QString(combination.data())), true, qApp);
    if (!handle->isRegistered())
    {
        std::print(std::cerr, "Failed to register hotkey: {}\n", combination);
        return;
    }
    QObject::connect(handle, &QHotkey::activated, qApp, std::forward<decltype(callback)>(callback));
}

void hotkey_handler::load_config()
{
    std::ifstream file(config_file.data());
    if (!file)
    {
        load_defaults();
        return;
    }

    auto j = nlohmann::json::parse(file);
    for (const auto &item : j)
    {
        std::string name = item["name"];
        std::string combination = item["combination"];
        std::function<void()> callback;
        if (name == "Activate")
            callback = std::bind(&hotkey_handler::make_prompt_window, this);
        else
        {
            std::print(std::cerr, "Unknown hotkey name: {}\n", name);
            continue;
        }

        M_hotkeys.emplace_back(name, combination, std::move(callback));
    }
}

void hotkey_handler::save_config()
{
    std::ofstream file(config_file.data());
    nlohmann::json j;
    for (const auto &hotkey : M_hotkeys)
    {
        j.push_back({
            {"name", hotkey.name},
            {"combination", hotkey.combination}
        });
    }
    file << j.dump(4);
}

void hotkey_handler::make_prompt_window()
{
    context ctx;
    if (auto res = sys::window::get_focused())
    {
        ctx.handle = std::move(*res);

        if (auto selected = ctx.handle.get_selected())
            ctx.selected_text = *selected |
                                std::views::drop_while([](char c) { return std::isspace(c); }) |
                                std::views::reverse | 
                                std::views::drop_while([](char c) { return std::isspace(c); }) |
                                std::views::reverse |
                                std::ranges::to<std::string>();
        else
            std::println("No text selected: {}", selected.error());

        if (auto focused = ctx.handle.get_screenshot())
            ctx.window = *std::move(focused);
        else
            std::println("No focused window: {}", focused.error());
    }
    else
        std::println("No focused window: {}", res.error());

    if (auto screen = sys::capture_screen())
        ctx.screen = *std::move(screen);
    else
        std::println("No screen captured: {}", screen.error());

    auto res = M_window_handler->create<prompt_window>(*M_ai, *M_window_handler, std::move(ctx));
    res->setAttribute(Qt::WA_DeleteOnClose);
    res->setWindowFlag(Qt::WindowStaysOnTopHint);
    res->raise();
    res->activateWindow();
    res->show();
}

void hotkey_handler::load_defaults()
{
    M_hotkeys.emplace_back("Activate", "Meta+Ctrl+R", std::bind(&hotkey_handler::make_prompt_window, this));
}