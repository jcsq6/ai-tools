#pragma once
#include <QWidget>
#include <memory>
#include <QSystemTrayIcon>
#include <QMenu>

QT_BEGIN_NAMESPACE
namespace Ui { class TrayWindow; }
QT_END_NAMESPACE

class tray_window : public QWidget
{
    Q_OBJECT
public:
    explicit tray_window(QWidget *parent = nullptr);
    ~tray_window();
private:
    std::unique_ptr<Ui::TrayWindow> ui;
};

class tray : public QWidget
{
    Q_OBJECT
public:
    explicit tray(QWidget *parent = nullptr);
    ~tray() = default;

private:
    tray_window *window;
    QSystemTrayIcon *icon;
    QMenu *menu;
};