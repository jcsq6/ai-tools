#include "uitools.h"
#include "ui_reword.h"

reword_window::reword_window(ai::reworder &reworder, ai::database &db, window_handler &handler) :
    ui_tool(reworder, db, handler),
    ui(new Ui::Reword)
{
    ui->setupUi(this);
}

reword_window::~reword_window() = default;