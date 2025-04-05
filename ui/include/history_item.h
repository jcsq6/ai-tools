#pragma once
#include <QWidget>
#include <memory>

#include "database.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class HistoryItem;
}
QT_END_NAMESPACE

class history_item : public QWidget
{
    Q_OBJECT
public:
    explicit history_item(const ai::database::entry &entry, QWidget *parent = nullptr);
    ~history_item();
private:
    std::unique_ptr<Ui::HistoryItem> M_ui;
};