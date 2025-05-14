#pragma once
#include <QPalette>
#include <QApplication>

inline void use_light_style(QApplication &app)
{
    constexpr auto window         = QColor(246, 244, 240);   // #F6F4F0
    constexpr auto panel          = QColor(247, 245, 242);   // #F7F5F2
    constexpr auto borderGrey     = QColor(180, 179, 178);   // #B4B3B2
    constexpr auto button         = QColor(249, 249, 244);   // #F9F9F4
    constexpr auto hover          = QColor(238, 236, 233);   //rgb(238, 236, 233)
    constexpr auto header     = QColor(226, 226, 226);   // #E2E2E2
    constexpr auto text      = Qt::black;
    constexpr auto highlight  = QColor(0, 122, 255);     // #007AFF

    QPalette pal;
    pal.setColor(QPalette::Window,           window);
    pal.setColor(QPalette::WindowText,       text);
    pal.setColor(QPalette::Base,             window);        // text‑entry background
    pal.setColor(QPalette::AlternateBase,    hover);         // table alt rows / hover
    pal.setColor(QPalette::ToolTipBase,      window);
    pal.setColor(QPalette::ToolTipText,      text);
    pal.setColor(QPalette::Text,             text);
    pal.setColor(QPalette::Button,           button);
    pal.setColor(QPalette::ButtonText,       text);
    pal.setColor(QPalette::BrightText,       Qt::red);
    pal.setColor(QPalette::Highlight,        highlight);
    pal.setColor(QPalette::HighlightedText,  text);
    pal.setColor(QPalette::PlaceholderText,  QColor(128,128,128));

    // Touch‑ups so Fusion respects header/hover shades
    pal.setColor(QPalette::Light,            header);
    pal.setColor(QPalette::Midlight,         hover);
    pal.setColor(QPalette::Shadow,           borderGrey);

    app.setPalette(pal);

    constexpr auto stylesheet = R"(
QPushButton:hover,
QToolButton:hover,
QCheckBox:hover,
QRadioButton:hover,
QAbstractItemView::item:hover,
QLineEdit:hover,
QSpinBox:hover,
QDoubleSpinBox:hover {
    background: rgba(230, 232, 232, 128);
})";
    app.setStyleSheet(stylesheet);
}