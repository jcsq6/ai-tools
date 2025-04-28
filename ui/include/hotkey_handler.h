#pragma once
#include <QApplication>
#include <string_view>
#include <vector>

#include "window_handler.h"
#include "ai_handler.h"

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

    hotkey_handler(window_handler &win_handle, ai_handler &ai) : M_window_handler{&win_handle}, M_ai(&ai)
    {
        load_config();
    }

    ~hotkey_handler()
    {
        save_config();
    }
private:
    std::vector<hotkey> M_hotkeys;
    window_handler *M_window_handler;
    ai_handler *M_ai;

    void make_prompt_window();

    void load_config();
    void save_config();
    void load_defaults();
};