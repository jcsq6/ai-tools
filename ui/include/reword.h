#pragma once
#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class Reword; }
QT_END_NAMESPACE

class reword_window : public QWidget
{
    Q_OBJECT
public:
    explicit reword_window(QWidget *parent = nullptr);
    ~reword_window();
private:
    std::unique_ptr<Ui::Reword> ui;
};