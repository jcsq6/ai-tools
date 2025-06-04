#include "database.h"

#include <expected>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <type_traits>

AI_BEG

std::string database::entry::date() const
{
    if (messages.empty())
        return "N/A";

    return (std::ostringstream() << std::put_time(std::localtime(&messages.front().created_at), "%m/%d-%Y")).str();
}

std::expected<database::entry *, std::string> database::append(thread &th)
{
    th.join();

    auto &messages = th.get_messages();
    if (messages.empty())
        return std::unexpected("No messages to save.");

    std::filesystem::path dbpath(M_path);
    std::filesystem::create_directories(dbpath);

    auto t1 = (std::ostringstream() << std::put_time(std::localtime(&messages.front().created_at), "%Y-%m-%d")).str();
    auto filename = dbpath / t1;
    filename.replace_extension(".json");

    nlohmann::json j;
    if (std::filesystem::exists(filename))
    {
        try
        {
            j = nlohmann::json::parse(std::ifstream(filename));
            if (!j.is_array())
                throw std::runtime_error("Database file is not an array.");
        }
        catch(const std::exception& e)
        {
            std::print(std::cerr, "Failed to parse database file: {}\n", e.what());
        }
    }

    std::ofstream file(filename);
    if (!file)
        return std::unexpected(std::format("Failed to open database file: {}", filename.string()));

    M_entries.push_back({
        .assistant = th.get_assistant().name(),
        .model = th.get_assistant().model(),
        .messages = messages
    });
    j.push_back({{"assistant", th.get_assistant().name()}, {"model", th.get_assistant().model()}, {"messages", nlohmann::json::array()}});
    auto &out_messages = j.back()["messages"];
    for (auto &message : messages)
    {
        out_messages.push_back({
            {"id", message.id},
            {"input", message.input},
            {"response", message.response},
            {"created_at", message.created_at}
        });
    }

    file << j.dump(4) << '\n';

    std::print("Saved thread to {}\n", filename.string());

    return &M_entries.back();
}

void database::load()
{
    std::print("Loading database from {}\n", M_path.string());
    for (const auto &file : std::filesystem::directory_iterator(M_path))
    {
        if (file.is_regular_file() && file.path().extension() == ".json")
        {
            try
            {
                auto j = nlohmann::json::parse(std::ifstream(file.path()));
                if (!j.is_array())
                    throw std::runtime_error("Database file is not an array.");
                for (const auto &item : j)
                {
                    if (!item.contains("assistant") || !item.contains("model") || !item.contains("messages"))
                        throw std::runtime_error("Database file is missing required fields.");
                    if (!item["messages"].is_array())
                        throw std::runtime_error("Database file messages field is not an array.");
                    entry e
                    {
                        .assistant = item["assistant"],
                        .model = item["model"]
                    };
                    e.messages.reserve(item["messages"].size());

                    auto get_or = [](const nlohmann::json &j, std::string_view key, auto &&default_value)
                    {
                        using default_type = std::remove_cvref_t<decltype(default_value)>;
                        try {
                            if (j.contains(key))
                                return j[key].get<default_type>();
                        }
                        catch (const std::exception &e)
                        {
                        }
                        return default_value;
                    };

                    for (const auto &message : item["messages"])
                        e.messages.push_back({
                            .id = get_or(message, "id", std::string()),
                            .input = get_or(message, "input", std::string()),
                            .response = get_or(message, "response", std::string()),
                            .created_at = get_or(message, "created_at", std::time_t(0))
                        });
                    M_entries.push_back(std::move(e));
                }
            }
            catch(const std::exception& e)
            {
                std::print(std::cerr, "Failed to load database file: {}\n", e.what());
            }
        }
    }

    std::ranges::sort(M_entries, {}, [](const entry &e) {
        return e.messages.empty() ? std::time_t(0) : e.messages.back().created_at;
    });
}
AI_END