#include "tray.h"
#include "ui_tray_window.h"

tray_window::tray_window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TrayWindow)
{
    ui->setupUi(this);
}

tray_window::~tray_window() = default;

tray::tray(QWidget *parent) :
    QWidget(parent)
{
    window = new tray_window();

    menu = new QMenu();
    menu->addAction("Settings", [&] { window->show(); });
    menu->addAction("Exit", [&] { QApplication::exit(); });

    icon = new QSystemTrayIcon(QIcon("assets/icon.png"));
    icon->setToolTip("AI-Tools");
    icon->show();
    icon->setContextMenu(menu);
}