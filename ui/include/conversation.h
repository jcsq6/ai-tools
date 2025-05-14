#pragma once

#include <QWidget>

#include <memory>

#include "ai_handler.h"
#include "database.h"


QT_BEGIN_NAMESPACE
namespace Ui {
    class Conversation;
}
QT_END_NAMESPACE

class conversation : public QWidget
{
    Q_OBJECT
public:
    explicit conversation(ai_handler &ai, QWidget *parent = nullptr, ai::database::entry *entry = nullptr);
    ~conversation();

    void add_message(const QString& text);
private:
    std::unique_ptr<Ui::Conversation> M_ui;

    ai_handler *M_ai;
    ai::database::entry *M_entry;
};