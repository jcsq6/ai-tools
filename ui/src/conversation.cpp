#include "conversation.h"
#include "ai.h"
#include "ui_conversation.h"

#include <QTextBrowser>
#include <QTime>
#include <QThread>

#include <concepts>
#include <functional>
#include <iostream>
#include <tuple>

class Bubble : public QTextBrowser
{
public:
    static constexpr int padding = 4;

    Bubble(bool user, QWidget *parent = nullptr) : QTextBrowser(parent), user{user}
    {
        setFrameStyle(QFrame::NoFrame);
        setReadOnly(true);
        setWordWrapMode(QTextOption::WordWrap);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        if (user)
            setStyleSheet(std::format("QTextBrowser {{ background:#E2E0E0; border-radius:10px; padding: {}px; }}", padding).c_str());
        else
            setStyleSheet(std::format("QTextBrowser {{ padding: {}px; }}", padding).c_str());
    }

    void resizeEvent(QResizeEvent *event) override
    {
        if (event)
            QTextBrowser::resizeEvent(event);

        if (user)
        {
            document()->setTextWidth(document()->idealWidth());
            setFixedWidth(document()->size().width() + padding * 2);
        }

        setFixedHeight(document()->size().height() + padding * 2);
    }

private:
    bool user;
};

auto make_callback(auto &&self, auto memfn)
{
    static auto maybe_to_string = [](auto arg)
    {
        if constexpr (std::same_as<decltype(arg), std::string_view>)
            return std::string(arg);
        else
            return arg;
    };

    return [&self, memfn](auto... args) {
        auto tuple_args = std::make_tuple(maybe_to_string(args)...);
        auto call = [&self, tuple_args, memfn]() mutable {
            std::apply([&self, memfn](auto&&... unpacked) {
                memfn(self, std::forward<decltype(unpacked)>(unpacked)...);
            }, tuple_args);
        };
        QMetaObject::invokeMethod(&self, call, Qt::QueuedConnection);
    };
};

conversation::conversation(ai_handler &ai, ai::thread &thread, QWidget *parent) :
    QWidget(parent),
    M_ui(new Ui::Conversation),
    M_ai(&ai),
    M_thread(&thread)
{
    M_ui->setupUi(this);

    for (auto &msg : thread.get_messages())
    {
        add_bubble(QString::fromStdString(msg.input));
        add_bubble(QString::fromStdString(msg.response));
    }

    M_stream = ai::text_stream_handler::make(ai::text_stream_handler::constructor_arg_t{
        .delta = make_callback(*this, std::mem_fn(&conversation::delta)),
        .finish = make_callback(*this, std::mem_fn(&conversation::finish)),
        .error = make_callback(*this, std::mem_fn(&conversation::error))
    });

    connect(M_ui->Send, &QToolButton::clicked, this, &conversation::send);
}

conversation::~conversation() = default;

void conversation::add_bubble(const QString &text, const QDateTime &time)
{
    auto vbox   = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    const bool user = vbox->count() % 2 == 0;

    // auto time_label = new QLabel(time.toString("hh:mm"));
    // time_label->setAlignment(Qt::AlignRight);
    // time_label->setStyleSheet("font-size: 10px; color: #A0A0A0;");
    // time_label->setContentsMargins(0, 0, 0, 0);

    auto bubble = new Bubble(user, M_ui->MessagesContent);
    bubble->setMarkdown(text);

    vbox->addWidget(bubble, 0, user ? Qt::AlignRight : Qt::Alignment());
}

void conversation::initial_send(std::string_view selected, std::string_view prompt)
{
    M_ui->Send->setEnabled(false);

    if (auto res = M_ai->ask().initial_send(*M_thread, *M_stream, M_files, prompt, selected))
    {
        add_bubble(QString::fromStdString(std::string(prompt)));
        add_bubble("");
        M_ui->PromptEdit->clear();
    }
    else
    {
        std::print(std::cerr, "Failed to send message: {}\n", res.error());
        M_ui->Send->setEnabled(true);
    }
}

void conversation::send()
{
    auto text = M_ui->PromptEdit->toPlainText();
    if (text.isEmpty())
        return;
    if (M_thread->is_running())
        return;
    
    M_ui->Send->setEnabled(false);

    if (auto res = M_ai->ask().send(*M_thread, *M_stream, M_files, text.toStdString()))
    {
        add_bubble(text); // user
        add_bubble(""); // response
        M_ui->PromptEdit->clear();
    }
    else
    {
        std::print(std::cerr, "Failed to send message: {}\n", res.error());
        M_ui->Send->setEnabled(true);
    }
}

void conversation::delta(std::string accum, std::string delta)
{
    if (accum.empty())
        return;

    auto vbox = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    auto bubble = static_cast<Bubble*>(vbox->itemAt(vbox->count() - 1)->widget());
    auto cursor = bubble->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QString::fromStdString(std::string(delta)));
    bubble->setTextCursor(cursor);
    bubble->ensureCursorVisible();
    bubble->resizeEvent(nullptr);
}

void conversation::finish(std::string accum)
{
    M_ui->Send->setEnabled(true);
    M_files.clear();
}

void conversation::error(ai::severity_t severity, std::string msg)
{
    switch (severity)
    {
    case ai::severity_t::info:
        std::print(std::cout, "Info: {}\n", msg);
        break;
    case ai::severity_t::warning:
        std::print(std::cerr, "Warning: {}\n", msg);
        break;
    case ai::severity_t::error:
        std::print(std::cerr, "Error: {}\n", msg);
        break;
    case ai::severity_t::fatal:
        std::print(std::cerr, "Fatal: {}\n", msg);
        M_ui->Send->setEnabled(true);
        break;
    }
}