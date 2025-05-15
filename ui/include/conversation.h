#pragma once

#include <QWidget>
#include <QDateTime>

#include <memory>
#include <qdatetime.h>
#include <string_view>

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

public slots:
    void add_bubble(const QString &text, const QDateTime &time = QDateTime::currentDateTime());
    void send(std::string_view text);
private:
    void delta(std::string_view accum, std::string_view delta);
    void finish(std::string_view accum);
    void error(ai::severity_t severity, std::string_view msg);

    std::unique_ptr<Ui::Conversation> M_ui;

    ai_handler *M_ai;
    ai::thread *M_thread;
    ai::text_stream_handler::handle_t M_stream;
};