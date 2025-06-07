#include "uitools.h"
#include "ai.h"
#include "ui_reword.h"
#include "ui_prompt_entry.h"

#include "system.h"

#include <QRadioButton>
#include <QMessageBox>

#include <QLineEdit>
#include <QTextEdit>
#include <array>
#include <expected>
#include <string_view>

#include <iostream>
#include <type_traits>

#include <QResource>
#include <vector>

struct ToolWindow_init
{
    ToolWindow_init()
    {
        Q_INIT_RESOURCE(ToolWindow);
    }
};

static ToolWindow_init init;

prompt_window::prompt_window(ai_handler &ai, window_handler &handler, context &&ctx) :
    QWidget(),
    M_context(std::move(ctx)),
    M_ai(&ai),
    M_handler(&handler),
    ui(new Ui::PromptEntry)
{
    static QPixmap not_found(":/assets/not-found.png");

    ui->setupUi(this);
    
    ui->SelectedText->setPlainText(QString::fromStdString(M_context.selected_text));
    if (M_context.selected_text.empty())
        ui->SelectedText->setPlaceholderText("No text selected");

    auto dpr = this->devicePixelRatioF();
    if (M_context.window.empty())
    {
        M_focused_disabled = true;
        M_window_image = not_found;
        M_window_image.setDevicePixelRatio(dpr);
    }
    else
    {
        auto in = QImage::fromData(M_context.window);
        in.setDevicePixelRatio(dpr);
        M_window_image = QPixmap::fromImage(in);
    }
    
    if (M_context.screen.empty())
    {
        M_screen_disabled = true;
        M_screen_image = not_found;
        M_screen_image.setDevicePixelRatio(dpr);
    }
    else
    {
        auto in = QImage::fromData(M_context.screen);
        in.setDevicePixelRatio(dpr);
        M_screen_image = QPixmap::fromImage(in);
    }

    ui->IncludeSelected->setChecked(!M_context.selected_text.empty());
    ui->IncludeFocused->setChecked(!M_context.window.empty());
    ui->IncludeScreen->setChecked(!M_context.screen.empty());

    ui->WindowScroll->setWidgetResizable(true);
    ui->ScreenScroll->setWidgetResizable(true);
    show();
    resizeEvent(nullptr);

    connect(ui->ToolSelector, &QComboBox::currentTextChanged, this, &prompt_window::set_inclusions);
    set_inclusions(ui->ToolSelector->currentText());

    connect(ui->PromptEdit, &QLineEdit::returnPressed, ui->Send, &QToolButton::click);
    connect(ui->Send, &QToolButton::clicked, [this]() {
        auto selected = ui->SelectedText->toPlainText();
        auto &window = M_context.window;
        auto &screen = M_context.screen;
        auto prompt = ui->PromptEdit->text();

        auto tool = ui->ToolSelector->currentText();

        auto setup = [&]{
            M_context.selected_text = selected.toStdString();

            if (!ui->IncludeFocused->isChecked())
                window.clear();
            if (!ui->IncludeScreen->isChecked())
                screen.clear();
        };

        auto make = [&]<typename T>(std::type_identity<T>)
        {
            auto res = M_handler->create<T>(*M_ai, *M_handler, std::move(M_context), prompt.toStdString());
            res->setAttribute(Qt::WA_DeleteOnClose);
            res->setWindowFlag(Qt::WindowStaysOnTopHint);
            res->raise();
            res->activateWindow();
            res->show();
        };

        if (tool == "Reword")
        {
            if (selected.isEmpty())
                return;

            setup();
            make(std::type_identity<reword_window>{});
        }
        else if (tool == "Create")
        {
            return;
        }
        else if (tool == "Ask")
        {
            setup();
            make(std::type_identity<ask_window>{});
        }
        else
            return;
        
        close();
    });

    connect(ui->SelectedText, &QTextEdit::textChanged, [this]() {
        set_inclusions(ui->ToolSelector->currentText());
    });
}

void prompt_window::options::enforce(prompt_window *window) const
{
    auto set = [=](priority p, QRadioButton *button, bool invalid) {
        switch (p)
        {
        case priority::optional_preferred:
        case priority::optional:
            button->setEnabled(!invalid);
            break;
        case priority::required:
            if (invalid)
                window->ui->Send->setEnabled(false);

            button->setChecked(true);
            button->setEnabled(false);
            break;
        case priority::disabled:
            button->setChecked(false);
            button->setEnabled(false);
            break;
        }
    };

    window->ui->Send->setEnabled(true);

    set(focused, window->ui->IncludeFocused, window->M_focused_disabled);
    set(selected, window->ui->IncludeSelected, window->ui->SelectedText->toPlainText().isEmpty() || window->M_selected_disabled);
    set(screen, window->ui->IncludeScreen, window->M_screen_disabled);
}

void prompt_window::options::enforce_preferred(prompt_window *window) const
{
    auto set = [=](priority p, QRadioButton *button, bool invalid) {
        switch (p)
        {
        case priority::optional_preferred:
            return button->setChecked(!invalid);
        case priority::optional:
            return button->setChecked(false);
        case priority::required:
            return button->setChecked(true);
        case priority::disabled:
            return button->setChecked(false);
        }
    };

    window->ui->Send->setEnabled(true);

    set(focused, window->ui->IncludeFocused, window->M_focused_disabled);
    set(selected, window->ui->IncludeSelected, window->ui->SelectedText->toPlainText().isEmpty() || window->M_selected_disabled);
    set(screen, window->ui->IncludeScreen, window->M_screen_disabled);
}

void prompt_window::set_inclusions(const QString &text)
{
    if (text == "Reword")
    {
        reword_options.enforce_preferred(this);
        reword_options.enforce(this);
    }
    else if (text == "Create")
    {
        create_options.enforce_preferred(this);
        create_options.enforce(this);
    }
    else if (text == "Ask")
    {
        ask_options.enforce_preferred(this);
        ask_options.enforce(this);
    }
}

void prompt_window::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    auto setup_image = [this](QScrollArea *scroll, QLabel *label, const QPixmap &image) {
        auto dpr = this->devicePixelRatioF();
        QSize vp = scroll->viewport()->size();
        QPixmap scaled = image.scaled(vp * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        label->setPixmap(scaled);
        label->setScaledContents(false);
        label->setAlignment(Qt::AlignCenter);

        label->setFixedSize(vp);
        scroll->widget()->setFixedSize(vp);

        if (auto L = qobject_cast<QLayout*>(scroll->widget()->layout()))
            L->setAlignment(label, Qt::AlignCenter);
    };

    setup_image(ui->WindowScroll, ui->WindowImage, M_window_image);
    setup_image(ui->ScreenScroll, ui->ScreenImage, M_screen_image);
}

prompt_window::~prompt_window() = default;

void ui_tool::finish()
{
    if (auto r = M_ai->database().append(*M_thread); !r)
        std::print(std::cerr, "Failed to append to database: {}\n", r.error());

    emit finished();
}

reword_window::reword_window(ai_handler &ai, window_handler &handler, context &&ctx, std::string_view prompt) :
    ui_tool(ai.reworder(), ai, handler, std::move(ctx)),
    ui(new Ui::Reword),
    M_stream_handler(ai::json_stream_handler::make({
        .delta = std::bind(&reword_window::on_delta, this, std::placeholders::_1),
        .finish = std::bind(&reword_window::on_finish, this, std::placeholders::_1)
    }))
{   
    ui->setupUi(this);
    ui->PromptEdit->setText(QString::fromUtf8(prompt.data()));

    connect(ui->PromptEdit, &QLineEdit::returnPressed, ui->Send, &QToolButton::click);
    connect(ui->Send, &QToolButton::clicked, [this] { send(); } );
    connect(ui->Accept, &QToolButton::clicked, [this] {
        auto revised = ui->RevisionText->toPlainText().toStdString();
        if (revised.empty())
            return;

        if (auto focus_res = M_context.handle.focus())
        {
            if (auto paste_res = sys::paste(revised))
            {
                close();
                return;
            }
            else
                std::print(std::cerr, "Failed to paste text: {}\n", paste_res.error());
        }
        else
            std::print(std::cerr, "Failed to focus window, copying instead: {}\n", focus_res.error());

        if (auto res = sys::copy(revised); !res)
        {
            std::print(std::cerr, "Failed to copy text: {}\n", res.error());
            return;
        }

        QMessageBox::warning(this, "Accept Failed", "Failed to paste or copy the revised text. Please try again.");
    });

    connect(ui->Copy, &QToolButton::clicked, [this] {
        auto revised = ui->RevisionText->toPlainText().toStdString();
        if (revised.empty())
            return;

        if (auto res = sys::copy(revised); !res)
            std::print(std::cerr, "Failed to copy text: {}\n", res.error());
    });

    ai::file::handle_t window;
    if (!M_context.window.empty())
    {
        if (auto res = ai::file::make(M_ai->client(), "window.jpg", M_context.window))
            window = std::move(res).value();
        else
            std::print(std::cerr, "Failed to process window file: {}\n", res.error());
    }

    ai::file::handle_t screen;
    if (!M_context.screen.empty())
    {
        if (auto res = ai::file::make(M_ai->client(), "screen.jpg", M_context.screen))
            screen = std::move(res).value();
        else
            std::print(std::cerr, "Failed to process screen file: {}\n", res.error());
    }

    if (auto res = M_ai->reworder().initial_send(*M_thread, *M_stream_handler, std::array{std::move(window), std::move(screen)}, prompt, M_context.selected_text); !res)
    {
        std::print(std::cerr, "Failed to send request: {}\n", res.error());
        return;
    }
}

void reword_window::send()
{
    if (auto res = M_ai->reworder().send(*M_thread, *M_stream_handler, std::views::empty<ai::file::handle_t>, ui->PromptEdit->text().toStdString()); !res)
    {
        std::print(std::cerr, "Failed to send request: {}\n", res.error());
        return;
    }

    ui->PromptEdit->setDisabled(true);
    ui->Send->setDisabled(true);
    ui->Accept->setDisabled(true);
    ui->Copy->setDisabled(true);
}

void reword_window::on_delta(const nlohmann::json &accum)
{
    auto fun = [this, accum]() 
    {
        auto improved = accum.contains("improved") && accum["improved"].is_string() ? accum["improved"].get<std::string>() : "";
        ui->RevisionText->setText(QString::fromStdString(improved));

        auto explanation = accum.contains("explanation") && accum["explanation"].is_string() ? accum["explanation"].get<std::string>() : "";
        ui->ExplanationText->setText(QString::fromStdString(explanation));
    };

    if (QThread::currentThread() == qApp->thread())
        fun();
    else
        QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}

void reword_window::on_finish(const nlohmann::json &accum)
{
    auto fun = [this, &accum]() 
    {
        ui->PromptEdit->clear();
        ui->PromptEdit->setDisabled(false);
        ui->Send->setDisabled(false);  
        ui->Accept->setDisabled(false);
        ui->Copy->setDisabled(false);
    };

    if (QThread::currentThread() == qApp->thread())
        fun();
    else
        QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}
reword_window::~reword_window() = default;

ask_window::ask_window(ai_handler &ai, window_handler &handler, context &&ctx, std::string_view prompt) :
    ui_tool(ai.ask(), ai, handler, std::move(ctx)),
    M_conversation(ai, *M_thread, this)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&M_conversation);

    if (!ctx.window.empty())
    {
        if (auto res = ai::file::make(ai.client(), "window.jpg", ctx.window))
            M_conversation.add_files(std::views::single(std::move(res).value()));
        else
            std::print(std::cerr, "Failed to process window file: {}\n", res.error());
    }

    if (!ctx.screen.empty())
    {
        if (auto res = ai::file::make(ai.client(), "screen.jpg", ctx.screen))
            M_conversation.add_files(std::views::single(std::move(res).value()));
        else
            std::print(std::cerr, "Failed to process screen file: {}\n", res.error());
    }

    M_conversation.initial_send(ctx.selected_text, prompt);
}
ask_window::~ask_window() = default;