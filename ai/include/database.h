#pragma once
#include "ai.h"
#include <expected>
#include <filesystem>
#include <vector>

AI_BEG

class database
{
public:
    struct entry
    {
        std::string assistant;
        std::string model;
        std::vector<thread::message> messages;

        std::string date() const;
    };

    database(std::filesystem::path path, bool do_load = true)
        : M_path(std::move(path))
    {
        if (std::filesystem::exists(M_path))
        {
            if (do_load)
                load();
        }
        else
            std::filesystem::create_directories(M_path);
    }

    std::expected<entry *, std::string> append(thread &th);

    const auto &get_entries() const { return M_entries; }
private:
    std::filesystem::path M_path;
    std::vector<entry> M_entries;

    void load();
};

AI_END