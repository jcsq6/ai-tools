#include <QApplication>
#include <entry.h>

#include <print>

#include "ai_handler.h"
#include "style.h"
#include "window_handler.h"
#include "hotkey_handler.h"

int app::run(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    use_light_style(app);

    ai_handler ai;
    window_handler windows(ai.database(), &app);
    hotkey_handler hotkeys(windows, ai);

    return app.exec();
}