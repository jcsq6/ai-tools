#include <QApplication>

#include <thread>
#include <chrono>
#include <print>

#include "reword.h"

#include "window_handler.h"
#include "hotkey_handler.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    hotkey_handler hotkey;

    ai::database db("database");
    window_handler handler(db, &app);

    return app.exec();
}