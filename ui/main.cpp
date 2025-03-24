#include <QApplication>

#include <thread>
#include <chrono>
#include <print>

#include "reword.h"

#include "window_handler.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    window_handler handler(&app);

    return app.exec();
}