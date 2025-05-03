#pragma once

#include <QWidget>
#include <string_view>
#include "ai.h"
#include "database.h"
#include "tools.h"
#include "window_handler.h"
#include "ai_handler.h"

#include "system.h"

struct context
{
    sys::window handle;
    std::string selected_text;
    std::vector<std::byte> window;
    std::vector<std::byte> screen;
};

QT_BEGIN_NAMESPACE
namespace Ui { class PromptEntry; }
QT_END_NAMESPACE

class prompt_window : public QWidget
{
    Q_OBJECT
public:
    explicit prompt_window(ai_handler &ai, window_handler &win_handler, context &&ctx);
    ~prompt_window();

protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    context M_context;
    QPixmap M_window_image;
    QPixmap M_screen_image;
    std::unique_ptr<Ui::PromptEntry> ui;
    ai_handler *M_ai;
    window_handler *M_handler;
};

class ui_tool : public QWidget
{
    Q_OBJECT
public:
    template <std::derived_from<ai::tool> Tool>
    ui_tool(Tool &tool, ai_handler &ai, window_handler &handler, context &&ctx) :
        QWidget(),
        M_ai(&ai),
        M_thread(tool.start_thread()),
        M_context(std::move(ctx))
    {
        connect(this, &ui_tool::finished, &handler.get_tray().get_window(), &tray_window::update_history, Qt::QueuedConnection);
    }
    virtual ~ui_tool() = default;

public slots:
    void finish()
    {
        M_ai->database().append(M_thread);
        emit finished();
    }

signals:
    void finished();

protected:
    ai_handler *M_ai;
    ai::thread M_thread;
    context M_context;
};

QT_BEGIN_NAMESPACE
namespace Ui { class Reword; }
QT_END_NAMESPACE

class reword_window : public ui_tool
{
    Q_OBJECT
public:
    explicit reword_window(ai_handler &ai, window_handler &handler, context &&ctx, std::string_view prompt);
    ~reword_window();
private:
    std::unique_ptr<Ui::Reword> ui;
    std::shared_ptr<ai::json_stream_handler> M_stream_handler;

    void on_delta(const nlohmann::json &accum);
    void on_finish(const nlohmann::json &accum);
    void send(std::string_view selected = {});
};