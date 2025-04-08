#pragma once
// #include <QMainWindow>
#include <memory>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QStandardItemModel>
#include <QStackedWidget>

#include "database.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class TrayWindow;
}
QT_END_NAMESPACE

class tray_window : public QWidget
{
    Q_OBJECT
public:
    explicit tray_window(const ai::database &db, QWidget *parent = nullptr);
    ~tray_window();

public slots:
    void update_history();

private slots:
    void on_conversation_double_clicked(const QModelIndex &index);

private:
    std::unique_ptr<Ui::TrayWindow> M_ui;
    QStandardItemModel *M_model;
    const ai::database *M_db;
};

class tray : public QWidget
{
    Q_OBJECT
public:
    explicit tray(const ai::database &db, QWidget *parent = nullptr);
    ~tray() = default;

private:
    tray_window *window;
    QSystemTrayIcon *icon;
    QMenu *menu;
};