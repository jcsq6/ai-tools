#include "tray.h"

#include "history_item.h"

#include "ui_history_item.h"
#include "ui_tray_window.h"

#include "tools.h"

#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <print>

#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QPainter>

class HtmlDelegate : public QStyledItemDelegate {
public:
    HtmlDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        QTextDocument doc;
        doc.setHtml(text);
    
        auto topt = doc.defaultTextOption();
        topt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(topt);
    
        // Set the document text width to the cell width to force wrapping
        doc.setTextWidth(option.rect.width());
    
        painter->save();
    
        // Draw background
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text = "";
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    
        // Translate painter and draw contents
        painter->translate(option.rect.topLeft());
        QRect clip(0, 0, option.rect.width(), option.rect.height());
        doc.drawContents(painter, clip);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QString text = index.data(Qt::DisplayRole).toString();
        
        QTextDocument doc;
        doc.setHtml(text);

        auto opt = doc.defaultTextOption();
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(opt);
        doc.setTextWidth(option.rect.width());
        
        return QSize(doc.idealWidth(), doc.size().height());
    }
};

Q_DECLARE_METATYPE(const ai::database::entry*)

auto plain_format(const std::string &response) {
    return std::format("<html>"
                       "<body>"
                       "<p style='font-size:11px; color: black;'>{}</p>"
                       "</body>"
                       "</html>",
                       response);
};

history_item::history_item(const ai::database::entry &entry, QWidget *parent) :
    QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint),
    M_ui(new Ui::HistoryItem)
{
    M_ui->setupUi(this);
    // connect(M_ui->Messages->horizontalHeader(), &QHeaderView::sectionResized, [this]() {
    //     M_ui->Messages->resizeColumnsToContents();
    // });
    M_ui->Messages->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    M_ui->Messages->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    M_ui->AssistantLabel->setText(QString::fromStdString(entry.assistant));
    M_ui->DateLabel->setText(QString::fromStdString(entry.model));
    M_ui->Messages->setItemDelegateForColumn(1, new HtmlDelegate(this));
    for (const auto &message : entry.messages) {

        QString response_string;
        if (entry.assistant == "Reworder")
            response_string = QString::fromStdString(ai::reworder::format(message.response));
        else
            response_string = QString::fromStdString(plain_format(message.response));

        M_ui->Messages->insertRow(M_ui->Messages->rowCount());
        M_ui->Messages->setItem(M_ui->Messages->rowCount() - 1, 0, new QTableWidgetItem(QString::fromStdString(message.input)));
        M_ui->Messages->setItem(M_ui->Messages->rowCount() - 1, 1, new QTableWidgetItem(response_string));
    }

    M_ui->Messages->resizeColumnsToContents();
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
                std::print("filtering with {}\n", column);
                if (M_ui->DateButton != button) M_ui->DateButton->setChecked(false);
                if (M_ui->ContentButton != button) M_ui->ContentButton->setChecked(false);
                if (M_ui->KeybindFilterButton != button) M_ui->KeybindFilterButton->setChecked(false);
            }
            else
            {
                proxy->setFilterKeyColumn(-1);
                std::print("filtering with all\n");
            }
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
    M_model->removeRows(0, M_model->rowCount());
    for (const auto &conversation : M_db->get_entries()) {
        auto date = new QStandardItem(QString::fromStdString(conversation.date()));

        auto content = new QStandardItem();
        content->setData(QString::fromStdString(conversation.messages[0].input), Qt::DisplayRole); // Display text

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
    window->show(); // remove

    menu = new QMenu(this);
    menu->addAction("Settings", [&] { window->show(); });
    menu->addAction("Exit", [&] { QApplication::exit(); });

    icon = new QSystemTrayIcon(QIcon("assets/icon.png"), this);
    icon->setToolTip("AI-Tools");
    icon->show();
    icon->setContextMenu(menu);
}