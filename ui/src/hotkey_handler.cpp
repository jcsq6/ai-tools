#include "hotkey_handler.h"

#include <fstream>
#include <print>
#include <iostream>

#include <json.hpp>

#include <QHotkey>

hotkey_handler::hotkey::hotkey(std::string_view name, std::string_view combination, auto &&callback)
    : name(name), combination(combination), handle{}
{
    if (combination.empty())
        return;

    handle = new QHotkey(QKeySequence(QString(combination.data())), true, qApp);
    if (!handle->isRegistered())
    {
        std::print(std::cerr, "Failed to register hotkey: {}\n", combination);
        return;
    }
    QObject::connect(handle, &QHotkey::activated, qApp, callback);
}

void hotkey_handler::load_config()
{
    std::ifstream file(config_file);
    if (!file)
    {
        load_defaults();
        return;
    }

    auto j = nlohmann::json::parse(file);
    for (const auto &item : j)
    {
        std::string name = item["name"];
        std::string combination = item["combination"];
        M_hotkeys.emplace_back(name, combination, [this, name]() {
            std::print("Hotkey {} activated\n", name);
        });
    }
}

void hotkey_handler::save_config()
{
    std::ofstream file(config_file);
    nlohmann::json j;
    for (const auto &hotkey : M_hotkeys)
    {
        j.push_back({
            {"name", hotkey.name},
            {"combination", hotkey.combination}
        });
    }
    file << j.dump(4);
}

void hotkey_handler::load_defaults()
{
    M_hotkeys.emplace_back("Reword", "Meta+Ctrl+R", [this]() {
        std::print("Default hotkey activated\n");
    });
}