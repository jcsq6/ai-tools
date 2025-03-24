#pragma once

#include <QWidget>
#include <QApplication>
#include <QMetaObject>
#include <memory>
#include <concepts>

struct widget_creator
{
    template <std::derived_from<QWidget> T, typename ...Args>
    static T *create(QApplication *app, Args&&... args)
    {
        T *widget;
        QMetaObject::invokeMethod(app, [&]{ widget = new T(std::forward<Args>(args)...); }, Qt::BlockingQueuedConnection);
        return widget;
    }
};