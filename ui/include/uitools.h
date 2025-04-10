#pragma once
#include <QWidget>
#include "database.h"
#include "tools.h"
#include "window_handler.h"

class ui_tool : public QWidget
{
    Q_OBJECT
public:
    template <std::derived_from<ai::tool> Tool>
    ui_tool(Tool &tool, ai::database &db, window_handler &handler) :
        QWidget(),
        M_db(&db),
        M_thread(tool.start_thread())
    {
        connect(this, &ui_tool::finished, &handler.get_tray().get_window(), &tray_window::update_history, Qt::QueuedConnection);
    }
    virtual ~ui_tool() = default;

public slots:
    void finish()
    {
        M_db->append(M_thread);
        emit finished();
    }

signals:
    void finished();

private:
    ai::database *M_db;
    ai::thread M_thread;
};

QT_BEGIN_NAMESPACE
namespace Ui { class Reword; }
QT_END_NAMESPACE

class reword_window : public ui_tool
{
    Q_OBJECT
public:
    explicit reword_window(ai::reworder &reworder, ai::database &db, window_handler &handler);
    ~reword_window();
private:
    std::unique_ptr<Ui::Reword> ui;
};