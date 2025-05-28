#include "tray.h"

#include "history_item.h"

#include "ui_history_item.h"
#include "ui_tray_window.h"

#include <qnamespace.h>
#include <qobject.h>
#include <string>
#include <print>

#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>

#include <QStyledItemDelegate>
#include <QAbstractTextDocumentLayout>
#include <QTextBrowser>
#include <QPainter>

struct Tray_init
{
    Tray_init()
    {
        Q_INIT_RESOURCE(TrayWindow);
    }
};

static Tray_init tray_init;

class MarkdownViewer : public QTextBrowser
{
public:
    explicit MarkdownViewer(const QString &md, QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setMarkdown(md);
        setOpenExternalLinks(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setReadOnly(true);
        setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        document()->setDocumentMargin(4);
        connect(document(), &QTextDocument::contentsChanged, this, &MarkdownViewer::updateGeometry);
        setStyleSheet("background-color: transparent;");
    }

    QSize sizeHint() const override
    {
        constexpr int fallbackWidth = 500;
        const qreal w = viewport()->width() > 0 ? viewport()->width() : fallbackWidth;
        QTextDocument *doc = document();
        doc->setTextWidth(w);
        const QSizeF s = doc->size();
        return { int(std::ceil(s.width())), int(std::ceil(s.height())) };
    }
};

Q_DECLARE_METATYPE(const ai::database::entry*)

auto title(std::string_view data)
{
    return data | std::views::chunk_by([](auto a, auto b) { return !std::isspace(a) && !std::isspace(b); }) |
            std::views::transform([](auto &&word) {
                auto res = word | std::ranges::to<std::string>();
                res[0] = std::toupper(res[0]);
                return res;
            }) |
            std::views::join | std::ranges::to<std::string>();
}

std::string to_markdown(const std::string &response, bool escape)
{
    auto get_str = [escape](std::string_view in) {
        auto str = in |
                           std::views::reverse |
                           std::views::drop_while([](char c) { return std::isspace(c); }) |
                           std::views::reverse | std::ranges::to<std::string>();

        if (!escape)
            return str;

        constexpr std::string_view escape_chars = "\\`*_{}[]<>()#+-.!|";
        std::string escaped;
        escaped.reserve(str.size() * 3 / 2);
        for (char c : str)
        {
            if (std::ranges::contains(escape_chars, c))
                escaped += '\\';
            escaped += c;
        }

        return escaped;
    };

    try
    {
        auto j = nlohmann::json::parse(response);

        std::ostringstream ss;
        for (const auto &item : j.items()) {
            ss << "## " << title(item.key()) << '\n';
            if (item.value().is_string())
                ss << get_str(item.value().get<std::string_view>()) << '\n';
            else if (item.value().is_array())
                for (const auto &sub_item : item.value())
                    ss << "- " << get_str(sub_item.get<std::string_view>()) << '\n';
        }

        return std::move(ss).str();
    }
    catch (const std::exception &)
    {
        return response;
    }
}

history_item::history_item(const ai::database::entry &entry, QWidget *parent) :
    QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint),
    M_ui(new Ui::HistoryItem)
{
    M_ui->setupUi(this);

    M_ui->AssistantLabel->setText(QString::fromStdString(entry.assistant));
    M_ui->DateLabel->setText(QString::fromStdString(entry.model));

    auto table = M_ui->Messages;
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    const auto make_item = [](const std::string &text, bool escape) {
        auto item = new MarkdownViewer(QString::fromStdString(to_markdown(text, escape)));
        return item;
    };
    for (const auto &message : entry.messages)
    {
        auto row = table->rowCount();
        table->insertRow(row);
        table->setCellWidget(row, 0, make_item(message.input, true));
        table->setCellWidget(row, 1, make_item(message.response, false));
    }
    
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->resizeRowsToContents();
    table->resizeColumnsToContents();
}
history_item::~history_item() = default;

class filter_proxy : public QSortFilterProxyModel {
public:
    filter_proxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int source_row, const QModelIndex& parent) const override
    {
        auto filter_column = [this, source_row, &parent](int col) {
            QModelIndex index = sourceModel()->index(source_row, col, parent);

            if (sourceModel()->data(index, Qt::DisplayRole).toString().contains(filterRegularExpression()))
                return true;
            
            auto json = sourceModel()->data(index, Qt::UserRole).toJsonObject();
            if (json.isEmpty())
                return false;
            auto assistant = json["assistant"].toString();
            auto messages = json["messages"].toArray();
            for (const auto& message : messages) {
                auto msg = message.toObject();
                if (msg["input"].toString().contains(filterRegularExpression()) ||
                    msg["response"].toString().contains(filterRegularExpression()))
                    return true;
            }

            return assistant.contains(filterRegularExpression()) ||
                   json["model"].toString().contains(filterRegularExpression());
        };

        if (filterKeyColumn() >= 0)
            return filter_column(filterKeyColumn());

        for (int col = 0; col < sourceModel()->columnCount(); ++col)
            if (filter_column(col))
                return true;
        return false;
    }
};

tray_window::tray_window(ai::database &db, QWidget *parent) :
    QWidget(parent),
    M_db(&db),
    M_ui(new Ui::TrayWindow)
{
    M_ui->setupUi(this);

    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);

    M_ui->HistoryButton->connect(M_ui->HistoryButton, &QPushButton::clicked, [&] {
        M_ui->Pages->setCurrentWidget(M_ui->HistoryPage);
    });

    M_ui->SettingsButton->connect(M_ui->SettingsButton, &QPushButton::clicked, [&] {
        M_ui->Pages->setCurrentWidget(M_ui->SettingsPage);
    });

    M_model = new QStandardItemModel(this);
    M_model->setColumnCount(2);
    M_model->setHeaderData(0, Qt::Horizontal, "Date");
    M_model->setHeaderData(1, Qt::Horizontal, "Content");
    M_ui->Conversations->verticalHeader()->setVisible(false);
    M_ui->Conversations->setColumnWidth(0, 100);
    M_ui->Conversations->setColumnWidth(1, 180 - 2);
    M_ui->Conversations->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto proxy = new filter_proxy(this);
    proxy->setSourceModel(M_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    M_ui->Conversations->setModel(proxy);
    // M_ui->Conversations->resizeColumnsToContents();
    proxy->setFilterKeyColumn(-1);

    QObject::connect(M_ui->SearchInput, &QLineEdit::textChanged, [proxy](const QString& text) {
        proxy->setFilterRegularExpression(text);
    });

    auto connect_filter = [proxy, this](QPushButton* button, int column) {
        QObject::connect(button, &QPushButton::clicked, [=, this] (bool checked) {
            if (checked)
            {
                proxy->setFilterKeyColumn(column);
                if (M_ui->DateButton != button) M_ui->DateButton->setChecked(false);
                if (M_ui->ContentButton != button) M_ui->ContentButton->setChecked(false);
                if (M_ui->KeybindFilterButton != button) M_ui->KeybindFilterButton->setChecked(false);
            }
            else
                proxy->setFilterKeyColumn(-1);
        });
    };

    connect_filter(M_ui->DateButton, 0);
    connect_filter(M_ui->ContentButton, 1);
    connect_filter(M_ui->KeybindFilterButton, 2);

    auto header = M_ui->Conversations->horizontalHeader();
    // header->setSectionResizeMode(QHeaderView::Fixed);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionsMovable(false);

    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);

    update_history();

    connect(M_ui->Conversations, &QTableView::doubleClicked, this, &tray_window::on_conversation_double_clicked);

}

tray_window::~tray_window() = default;

void tray_window::update_history()
{
    auto extract = [](std::string_view input, std::string_view assistant) {
        try
        {
            auto json = nlohmann::json::parse(input);
            if (assistant == "Reworder")
            {
                if (json.contains("Selected") && json["Selected"].is_string())
                    return json["Selected"].get<std::string>();
            }
            else if (assistant == "Ask")
            {
                if (json.contains("Prompt") && json["Prompt"].is_string())
                    return json["Prompt"].get<std::string>();
            }
            
            for (const auto &item : json.items()) {
                if (item.value().is_string())
                    return item.value().get<std::string>();
                else if (item.value().is_array())
                    for (const auto &sub_item : item.value())
                        return sub_item.get<std::string>();
            }

            throw 0;
        }
        catch (...)
        {
            return std::string(input);
        }
    };
    
    M_model->removeRows(0, M_model->rowCount());
    for (const auto &conversation : M_db->get_entries()) {
        auto date = new QStandardItem(QString::fromStdString(conversation.date()));

        auto content = new QStandardItem();
        content->setData(QString::fromStdString(extract(conversation.messages[0].input, conversation.assistant)), Qt::DisplayRole); // Display text

        QJsonObject json;
        json["assistant"] = QString::fromStdString(conversation.assistant);
        json["model"] = QString::fromStdString(conversation.model);
        QJsonArray messages;
        for (auto message : conversation.messages) {
            QJsonObject msg;
            msg["input"] = QString::fromStdString(message.input);
            msg["response"] = QString::fromStdString(message.response);
            messages.append(msg);
        }
        json["messages"] = messages;
        content->setData(json, Qt::UserRole); // Store metadata

        QVariant entryPointer = QVariant::fromValue(&conversation);
        content->setData(entryPointer, Qt::UserRole + 1); // Use a custom role

        M_model->appendRow({ date, content });
    }

    M_model->sort(0, Qt::DescendingOrder);
}

void tray_window::on_conversation_double_clicked(const QModelIndex &index)
{
    auto proxy = qobject_cast<QSortFilterProxyModel*>(M_ui->Conversations->model());
    if (!proxy)
        return;

    QModelIndex sourceIndex = proxy->mapToSource(index);

    auto content = M_model->item(sourceIndex.row(), 1)->data(Qt::UserRole + 1).value<const ai::database::entry*>();
    if (!content)
    {
        std::print("Failed to get find database entry\n");
        return;
    }

    auto *item = new history_item(*content, this);
    item->setAttribute(Qt::WA_DeleteOnClose);
    item->setAttribute(Qt::WA_ShowWithoutActivating);
    item->raise();
    item->show();
}

tray::tray(ai::database &db, QWidget *parent) :
    QWidget(parent)
{
    window = new tray_window(db);
    // window->show(); // remove

    menu = new QMenu(this);
    menu->addAction("Settings", [&] { window->show(); window->raise(); window->activateWindow(); });
    menu->addAction("Exit", [&] { QApplication::exit(); });

    icon = new QSystemTrayIcon(QIcon("assets/icon.png"), this);
    icon->setToolTip("AI-Tools");
    icon->show();
    icon->setContextMenu(menu);
}