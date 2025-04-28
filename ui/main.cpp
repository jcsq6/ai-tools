#include <QApplication>

#include <thread>
#include <chrono>
#include <print>

#include "window_handler.h"
#include "hotkey_handler.h"

#include <entry.h>

int app::run(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    ai_handler ai;
    window_handler windows(ai.database(), &app);
    hotkey_handler hotkeys(windows, ai);

    return app.exec();
}