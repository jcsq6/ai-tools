#pragma once

#include <QWidget>

#include <string_view>

#include "ai.h"
#include "tools.h"
#include "window_handler.h"
#include "ai_handler.h"
#include "conversation.h"

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
    void set_inclusions(const QString &text);
    void resizeEvent(QResizeEvent *event) override;
private:
    context M_context;
    QPixmap M_window_image;
    QPixmap M_screen_image;
    std::unique_ptr<Ui::PromptEntry> ui;
    ai_handler *M_ai;
    window_handler *M_handler;

    bool M_selected_disabled = false;
    bool M_focused_disabled = false;
    bool M_screen_disabled = false;

    struct options
    {
        // optional will start out as unchecked 
        enum class priority { optional, optional_preferred, required, disabled };
        priority selected = priority::optional;
        priority focused = priority::optional;
        priority screen = priority::optional;

        void enforce(prompt_window *window) const;
        void enforce_preferred(prompt_window *window) const;
    };

    static constexpr options reword_options = {
        .selected = options::priority::required,
        .focused = options::priority::optional_preferred,
        .screen = options::priority::optional
    };

    static constexpr options create_options = {
        .selected = options::priority::optional,
        .focused = options::priority::optional_preferred,
        .screen = options::priority::optional
    };

    static constexpr options ask_options = {
        .selected = options::priority::optional,
        .focused = options::priority::optional,
        .screen = options::priority::optional_preferred
    };
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
    void finish();

signals:
    void finished();

protected:
    void closeEvent(QCloseEvent *event) override
    {
        finish();
        QWidget::closeEvent(event);
    }

    ai_handler *M_ai;
    ai::thread::handle_t M_thread;
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
    ai::json_stream_handler::handle_t M_stream_handler;

    void on_delta(const nlohmann::json &accum);
    void on_finish(const nlohmann::json &accum);
    void send();
};

class ask_window : public ui_tool
{
    Q_OBJECT
public:
    explicit ask_window(ai_handler &ai, window_handler &handler, context &&ctx, std::string_view prompt);
    ~ask_window();
private:
    conversation M_conversation;
};