#pragma once

#include <QWidget>
#include <QDateTime>

#include <memory>

#include "ai.h"
#include "ai_handler.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class Conversation;
}
QT_END_NAMESPACE

class conversation : public QWidget
{
    Q_OBJECT
public:
    // TODO: allow loading in old conversations (entry provided here for testing, will be used in the future to load old threads)
    explicit conversation(ai_handler &ai, ai::thread &thread, QWidget *parent = nullptr);
    ~conversation();

    template <std::ranges::range R> requires(std::same_as<std::ranges::range_value_t<std::remove_cvref_t<R>>, ai::file::handle_t>)
    void add_files(R &&files)
    {
        M_files.append_range(std::forward<R>(files));
    }

    void initial_send(std::string_view selected, std::string_view prompt);

public slots:
    void add_bubble(const QString &text, const QDateTime &time = QDateTime::currentDateTime());
    void send();
private:
    void delta(std::string accum, std::string delta);
    void finish(std::string accum);
    void error(ai::severity_t severity, std::string msg);

    std::unique_ptr<Ui::Conversation> M_ui;
    std::vector<ai::file::handle_t> M_files;

    ai_handler *M_ai;
    ai::thread *M_thread;
    ai::text_stream_handler::handle_t M_stream;
};