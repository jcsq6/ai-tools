#pragma once
#include <QApplication>
#include <string_view>
#include <vector>

class QHotkey;

class hotkey_handler
{
    struct hotkey
    {
        hotkey(std::string_view name, std::string_view combination, auto &&callback);
        std::string name;
        std::string combination;
        QHotkey *handle;
    };
public:
    static constexpr std::string_view config_file = "hotkey-config.json";

    hotkey_handler()
    {
        load_config();
    }

    ~hotkey_handler()
    {
        save_config();
    }
private:
    std::vector<hotkey> M_hotkeys;

    void load_config();
    void save_config();
    void load_defaults();
};