#include "winsys.h"
#include <exception>

SYS_BEG

result<std::vector<std::byte>> capture_screen()
{
    return std::unexpected("Capture screen not implemented");
}

result<void> copy(std::string_view text)
{
    return std::unexpected("Copy to clipboard not implemented");
}

result<void> paste(std::string_view text)
{
    return std::unexpected("Paste to clipboard not implemented");
}

window::window() : M_handle() 
{
}

result<std::string> window::get_selected() const
{
    return std::unexpected("Get selected text not implemented");
}
result<std::vector<std::byte>> window::get_screenshot() const
{
    return std::unexpected("Get screenshot not implemented");
}
result<void> window::focus() const
{
    return std::unexpected("Focus window not implemented");
}
result<std::string> window::get_name() const
{
    return std::unexpected("Get window name not implemented");
}

window::~window() = default;

result<window> window::get_focused()
{
    return std::unexpected("Get focused window not implemented");
}

window::window(window&&) = default;
window& window::operator=(window&&) = default;
SYS_END