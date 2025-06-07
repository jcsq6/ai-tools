#include "conversation.h"
#include "ai.h"
#include "ui_conversation.h"

#include <QTextBrowser>
#include <QTime>
#include <QThread>
#include <QRegularExpression>
#include <QBuffer>

#include <jkqtmathtext.h>

#include <concepts>
#include <functional>
#include <iostream>
#include <optional>
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
        // setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        if (user)
            setStyleSheet(QString::fromStdString(std::format("QTextBrowser {{ background:#E2E0E0; border-radius:10px; padding: {}px; }}", padding)));
        else
            setStyleSheet(QString::fromStdString(std::format("QTextBrowser {{ padding: {}px; }}", padding)));

        if (auto p = parentWidget())
            p->installEventFilter(this);
    }

    bool isUser() const
    {
        return user;
    }

    void setContent(std::string_view text)
    {
        setMarkdown(QString::fromStdString(std::string(text)));
        updateGeometry();
        setFixedHeight(document()->size().height() + padding * 2);
    }

    void setMathContent(std::string_view text)
    {
        char last{};
        char first{};
        using namespace std::literals;
        constexpr std::array<std::tuple<std::string_view, std::string_view, bool>, 6> inline_delims = {{
            {"\\(", "\\)", false}, // inline
            {"$", "$", false},
            {"\\begin{math}", "\\end{math}", false},
            {"\\[", "\\]", true}, // display
            {"\\begin{displaymath}", "\\end{displaymath}", true},
            {"\\begin{equation}", "\\end{equation}", true},
        }};

        std::string markdown;
        markdown.reserve(text.size());

        std::vector<std::pair<QString, QString>> places;

        auto get_replacement = [this](std::string_view math_text, bool display) -> std::optional<QString>
        {
            JKQTMathText math;
            math.useXITS();
            if (!math.parse(QString::fromStdString(std::string(math_text))))
                return std::nullopt;

            double base_point = QFontInfo(font()).pointSizeF();
            math.setFontSize(base_point * 1.2);
            auto image = math.drawIntoImage(false, Qt::transparent);

            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");

            QString img_tag = QString("<img src=\"data:image/png;base64,%1\">").arg(QString::fromLatin1(png.toBase64()));

            if (display)
                return QString("<div style='text-align:center;'>%1</div>").arg(img_tag);
            else
                return img_tag;
        };
        
        char prev{};
        // assume there are no unescaped unclosed delimiters
        for (auto it = text.begin(); it != text.end();)
        {
            if (prev != '\\')
                if (auto delim = std::ranges::find_if(inline_delims, [it](const auto &pair) { return std::get<0>(pair) == std::string_view(it, std::get<0>(pair).size()); }); delim != inline_delims.end())
                {
                    auto [open, close, display] = *delim;
                    std::string_view after = it + open.size();
                    if (auto end = after.find(close); end != std::string_view::npos && after[end - 1] != '\\')
                    {
                        auto math = std::string_view(after.begin(), after.begin() + end);

                        auto inln = QString("MATH{%1}").arg(std::hash<std::string_view>{}(math));
                        auto repl = get_replacement(math, display);
                        if (repl)
                        {
                            places.emplace_back(inln, *std::move(repl));
                            it = after.begin() + end + close.size();
        
                            markdown.append(inln.toStdString());
                            prev = {};
                            continue;
                        }
                    }
                }
            
            prev = *(it++);
            markdown.push_back(prev);
        }

        QTextDocument doc;
        doc.setMarkdown(QString::fromUtf8(markdown.c_str()));

        auto html = doc.toHtml();
        for (auto [inln, repl] : places)
            html.replace(inln, repl);

        setHtml(html);
        updateGeometry();
        setFixedHeight(document()->size().height() + padding * 2);
    }

    QSize sizeHint() const override
    {
        if (!user)
            return QTextBrowser::sizeHint();

        const int parent_width = parentWidget() ? parentWidget()->width() : 400; // Default width if no parent
        const float min_width = int(.4 * parent_width);

        QTextDocument *doc = document();
        const int desired = qMax(min_width - padding * 2, doc->idealWidth());
        doc->setTextWidth(desired);

        QSize size(doc->size().toSize().width() + padding * 2, -1);
        return size;
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QTextBrowser::resizeEvent(event);
        setFixedHeight(document()->size().height() + padding * 2);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            updateGeometry();
        return QTextBrowser::eventFilter(watched, event);
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

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    for (auto &msg : thread.get_messages())
    {
        add_bubble(msg.input); // TODO: parse json input
        add_bubble(msg.response);
    }

    M_stream = ai::text_stream_handler::make(ai::text_stream_handler::constructor_arg_t{
        .delta = make_callback(*this, std::mem_fn(&conversation::delta)),
        .finish = make_callback(*this, std::mem_fn(&conversation::finish)),
        .error = make_callback(*this, std::mem_fn(&conversation::error))
    });

    connect(M_ui->Send, &QToolButton::clicked, this, &conversation::send);
}

conversation::~conversation() = default;

void conversation::add_bubble(std::string_view text, bool parse_math, const QDateTime &time)
{
    auto vbox   = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    const bool user = vbox->count() % 2 == 0;

    auto bubble = new Bubble(user, M_ui->MessagesContent);
    if (parse_math)
        bubble->setMathContent(text);
    else
        bubble->setContent(text);

    vbox->addWidget(bubble, 0, user ? Qt::AlignRight : Qt::Alignment());
}

void conversation::initial_send(std::string_view selected, std::string_view prompt)
{
    M_ui->Send->setEnabled(false);

    if (auto res = M_ai->ask().initial_send(*M_thread, *M_stream, M_files, prompt, selected))
    {
        add_bubble(prompt);
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

    auto text_str = text.toStdString();

    if (auto res = M_ai->ask().send(*M_thread, *M_stream, M_files, text_str))
    {
        add_bubble(text_str); // user
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
    bubble->setContent(accum);
}

void conversation::finish(std::string accum)
{
    M_ui->Send->setEnabled(true);
    M_files.clear();

    auto vbox = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
    auto bubble = static_cast<Bubble*>(vbox->itemAt(vbox->count() - 1)->widget());
    bubble->setMathContent(accum); // TODO: parse math realtime
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
        // TODO: re-enable loaded files
        std::print(std::cerr, "Fatal: {}\n", msg);
        {
            auto vbox = static_cast<QVBoxLayout*>(M_ui->MessagesContent->layout());
            vbox->removeItem(vbox->itemAt(vbox->count() - 1));
        }
        M_ui->Send->setEnabled(true);
        break;
    }
}