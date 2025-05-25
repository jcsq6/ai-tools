#include <QApplication>

#include <initializer_list>
#include <print>
#include <iostream>
#include <ranges>

#include "entry.h"
#include "ai_handler.h"
#include "include/style.h"
#include "system.h"
#include "window_handler.h"
#include "uitools.h"

#include "style.h"

int app::run(int argc, char **argv)
{
    if (argc < 3)
    {
        std::print(std::cerr, "Usage: {} <reword|ask|create> <delay> [prompt]\n", argv[0]);
        return 1;
    }

    auto args = std::ranges::subrange(argv, argv + argc) | std::views::drop(1) | std::ranges::to<std::vector<std::string_view>>();
    if (!std::ranges::contains(std::initializer_list{"reword", "ask", "create", "settings"}, args[0]))
    {
        std::print(std::cerr, "Invalid type: {}\n", args[0]);
        return 1;
    }

    double delay = 0;
    try
    {
        delay = std::stoi(std::string(args[1]));
    }
    catch (const std::exception &e)
    {
        std::print(std::cerr, "Invalid delay: {}\n", e.what());
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));

    std::string prompt(args.size() > 2 ? args[2] : "");

    QApplication app(argc, argv);
    use_light_style(app);
    app.setQuitOnLastWindowClosed(true);

    ai_handler ai;
    window_handler windows(ai.database(), &app);

    auto get_ctx = [&ai, &windows]() {
        context ctx;
        
        if (auto res = sys::window::get_focused(); !res)
            std::print(std::cerr, "Failed to get focused window: {}\n", res.error());
        else
            ctx.handle = std::move(res).value();

        if (auto res = sys::capture_screen(); !res)
            std::print(std::cerr, "Failed to get screen: {}\n", res.error());
        else
            ctx.screen = std::move(res).value();

        if (auto res = ctx.handle.get_screenshot(); !res)
            std::print(std::cerr, "Failed to get window: {}\n", res.error());
        else
            ctx.window = std::move(res).value();

        if (auto res = ctx.handle.get_selected(); !res)
            std::print(std::cerr, "Failed to get selected text: {}\n", res.error());
        else
            ctx.selected_text = std::move(res).value();

        return ctx;
    };

    if (args[0] == "ask")
    {
        ask_window *window = new ask_window(ai, windows, get_ctx(), prompt);
        window->raise();
        window->setWindowFlag(Qt::WindowStaysOnTopHint);
        window->activateWindow();
        window->show();
    }
    else if (args[0] == "reword")
    {
        reword_window *window = new reword_window(ai, windows, get_ctx(), prompt);
        window->raise();
        window->setWindowFlag(Qt::WindowStaysOnTopHint);
        window->activateWindow();
        window->show();
    }
    else if (args[0] == "create")
    {
        // create_window *window = new create_window(ai, windows, get_ctx(), prompt);
        // window->show();
        return 0;
    }
    else if (args[0] == "settings")
    {
        windows.get_tray().get_window().show();
        windows.get_tray().get_window().raise();
        windows.get_tray().get_window().activateWindow();
    }

    return app.exec();
}