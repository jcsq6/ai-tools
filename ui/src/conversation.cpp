#include "conversation.h"
#include "database.h"
#include "ui_conversation.h"

#include <QTextBrowser>
#include <QTime>

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

conversation::conversation(ai_handler &ai, QWidget *parent, ai::database::entry *entry) :
    QWidget(parent),
    M_ui(new Ui::Conversation),
    M_ai(&ai),
    M_entry(entry)
{
    M_ui->setupUi(this);

    add_message("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed euismod, urna eu tincidunt consectetur, nisi nisl aliquam enim, eget facilisis quam felis id mauris. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Suspendisse potenti. Etiam euismod, justo at facilisis cursus, enim erat dictum urna, nec dictum erat urna nec erat.");
    add_message("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed euismod, urna eu tincidunt consectetur, nisi nisl aliquam enim, eget facilisis quam felis id mauris. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Suspendisse potenti. Etiam euismod, justo at facilisis cursus, enim erat dictum urna, nec dictum erat urna nec erat.");
    add_message("This is a test message.");
    add_message("This is another test message.");
    add_message("From the user.");
}

conversation::~conversation() = default;

void conversation::add_message(const QString &text)
{
    auto vbox   = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    const bool user = vbox->count() % 2 == 0;

    auto bubble = new Bubble(user, M_ui->MessagesContent);
    bubble->setMarkdown(text);

    vbox->addWidget(bubble, 0, user ? Qt::AlignRight : Qt::Alignment());
}