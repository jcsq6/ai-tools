#pragma once
#include <QPalette>

inline QPalette light_palette()
{
    constexpr auto window         = QColor(246, 244, 240);   // #F6F4F0
    constexpr auto panel          = QColor(247, 245, 242);   // #F7F5F2
    constexpr auto borderGrey     = QColor(180, 179, 178);   // #B4B3B2  (used by frames via stylesheet)
    constexpr auto button         = QColor(249, 249, 244);   // #F9F9F4
    constexpr auto hover          = QColor(238, 236, 233);   // #EEECE9
    constexpr auto headerGrey     = QColor(226, 226, 226);   // #E2E2E2
    constexpr auto textBlack      = Qt::black;
    constexpr auto highlightBlue  = QColor(0, 122, 255);     // #007AFF

    QPalette pal;
    pal.setColor(QPalette::Window,           window);
    pal.setColor(QPalette::WindowText,       textBlack);
    pal.setColor(QPalette::Base,             window);        // text‑entry background
    pal.setColor(QPalette::AlternateBase,    hover);         // table alt rows / hover
    pal.setColor(QPalette::ToolTipBase,      window);
    pal.setColor(QPalette::ToolTipText,      textBlack);
    pal.setColor(QPalette::Text,             textBlack);
    pal.setColor(QPalette::Button,           button);
    pal.setColor(QPalette::ButtonText,       textBlack);
    pal.setColor(QPalette::BrightText,       Qt::red);
    pal.setColor(QPalette::Highlight,        highlightBlue);
    pal.setColor(QPalette::HighlightedText,  Qt::white);
    pal.setColor(QPalette::PlaceholderText,  QColor(128,128,128));

    // Touch‑ups so Fusion respects header/hover shades
    pal.setColor(QPalette::Light,            headerGrey);
    pal.setColor(QPalette::Midlight,         hover);
    pal.setColor(QPalette::Shadow,           borderGrey);

    return pal;
}