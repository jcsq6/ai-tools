#include "uitools.h"
#include "ai.h"
#include "ui_reword.h"
#include "ui_prompt_entry.h"

#include "system.h"

#include <QRadioButton>
#include <QMessageBox>

#include <string_view>

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

    connect(ui->Send, &QToolButton::clicked, this, [this]() {
        auto selected = ui->SelectedText->toPlainText();
        auto &window = M_context.window;
        auto &screen = M_context.screen;
        auto prompt = ui->PromptEdit->text();

        auto tool = ui->ToolSelector->currentText();
        if (tool == "Reword")
        {
            if (selected.isEmpty())
                return;

            M_context.selected_text = selected.toStdString();

            if (!ui->IncludeFocused->isChecked())
                window.clear();
            if (!ui->IncludeScreen->isChecked())
                screen.clear();

            auto res = M_handler->create<reword_window>(*M_ai, *M_handler, std::move(M_context), prompt.toStdString());
            res->setAttribute(Qt::WA_DeleteOnClose);
            res->setWindowFlag(Qt::WindowStaysOnTopHint);
            res->raise();
            res->activateWindow();
            res->show();
        }
        else if (tool == "Create")
        {

        }
        else if (tool == "Ask")
        {

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
        case priority::optional:
            button->setEnabled(!invalid);
            break;
        case priority::required:
            if (invalid)
                window->ui->Send->setEnabled(false);
            else
            {
                button->setChecked(true);
                button->setEnabled(false);
            }

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

void prompt_window::set_inclusions(const QString &text)
{
    if (text == "Reword")
        reword_options.enforce(this);
    else if (text == "Create")
        create_options.enforce(this);
    else if (text == "Ask")
        ask_options.enforce(this);
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

    send(M_context.selected_text);
}

void reword_window::send(std::string_view selected)
{
    if (auto res = M_ai->reworder().send(M_thread, selected, ui->PromptEdit->toPlainText().toStdString(), M_context.window, M_stream_handler); !res)
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
    auto fun = [this, accum]() 
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