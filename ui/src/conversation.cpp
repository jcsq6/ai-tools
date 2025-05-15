#include "conversation.h"
#include "ai.h"
#include "ui_conversation.h"

#include <QTextBrowser>
#include <QTime>
#include <QThread>

#include <iostream>

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

    auto make_callback = [this](auto memfn) {
        auto fun = [this, memfn](auto &&...args)
        {
            if (QThread::currentThread() == qApp->thread())
                (this->*memfn)(std::forward<decltype(args)>(args)...);
            else
                QMetaObject::invokeMethod(this, [&]() { (this->*memfn)(std::forward<decltype(args)>(args)...); }, Qt::QueuedConnection);
        };
        return fun;
    };

    M_stream = ai::text_stream_handler::make(ai::text_stream_handler::constructor_arg_t{
        .delta = make_callback(&conversation::delta),
        .finish = make_callback(&conversation::finish),
        .error = make_callback(&conversation::error)
    });

    connect(M_ui->Send, &QToolButton::clicked, [this]() {
        auto text = M_ui->PromptEdit->toPlainText();
        send(text.toStdString());
    });
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

void conversation::send(std::string_view text)
{
    if (text.empty())
        return;
    if (M_thread->is_running())
        return;
    
    M_ui->Send->setEnabled(false);

    if (auto res = M_ai->ask().send(*M_thread, ai::input{ .prompt = text }, *M_stream))
    {
        add_bubble(QString::fromStdString(std::string(text))); // user
        add_bubble(""); // response
        M_ui->PromptEdit->clear();
    }
    else
    {
        std::print(std::cerr, "Failed to send message: {}\n", res.error());
        M_ui->Send->setEnabled(true);
    }
}

void conversation::delta(std::string_view accum, std::string_view delta)
{
    if (accum.empty())
        return;

    auto vbox = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    auto bubble = static_cast<Bubble*>(vbox->itemAt(vbox->count() - 1)->widget());
    bubble->append(QString::fromStdString(std::string(delta)));
}

void conversation::finish(std::string_view accum)
{
    M_ui->Send->setEnabled(true);
}

void conversation::error(ai::severity_t severity, std::string_view msg)
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