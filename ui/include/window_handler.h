#pragma once
#include "tray.h"

#include <vector>
#include <QSystemTrayIcon>

class window_handler
{
public:
    window_handler(const ai::database &db, QApplication *app) : M_app(app)
    {
        M_tray = new tray(db);
    }
private:
    std::vector<QWidget *> M_windows;
    tray *M_tray;
    QApplication *M_app;
};