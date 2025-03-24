#include "reword.h"
#include "ui_reword.h"

reword_window::reword_window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Reword)
{
    ui->setupUi(this);
}

reword_window::~reword_window() = default;