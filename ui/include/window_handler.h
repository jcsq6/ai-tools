#pragma once
#include "tray.h"

#include <QWidget>
#include <QMetaObject>
#include <QThread>
#include <QSystemTrayIcon>
#include <QApplication>

#include <concepts>
#include <vector>

class window_handler
{
public:
    window_handler(ai::database &db, QApplication *app) : M_app(app)
    {
        M_tray = new tray(db);
    }

    template <std::derived_from<QWidget> T, typename ...Args>
    T *create(Args&&... args)
    {
        T *widget = nullptr;
        if (QThread::currentThread() == M_app->thread())
            widget = new T(std::forward<Args>(args)...);
        else
            QMetaObject::invokeMethod(M_app, [&]{ widget = new T(std::forward<Args>(args)...); }, Qt::BlockingQueuedConnection);
        M_windows.push_back(widget);
        return widget;
    }

    auto &database() const { return M_tray->database(); }

    auto &get_tray() const { return *M_tray; }
private:
    std::vector<QWidget *> M_windows;
    tray *M_tray;
    QApplication *M_app;
};