#pragma once
#include <expected>
#include <span>
#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "ai.h"

AI_BEG

class file : public detail::shared<file>
{
public:
    static std::expected<handle_t, std::string> make(handle &client, const std::filesystem::path &filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file)
            return std::unexpected(std::format("Failed to open file {}", filename.string()));

        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::byte> buff(size);
        file.read(reinterpret_cast<char *>(buff.data()), size);

        return make(client, filename, buff);
    }

    template <std::ranges::contiguous_range R>
    static std::expected<handle_t, std::string> make(handle &client, const std::filesystem::path &filename, R &&bytes)
    {
        if (std::ranges::empty(bytes))
            return std::unexpected(std::format("File {} is empty", filename.string()));

        if (auto res = process(client, std::as_bytes(std::span(bytes)), filename))
            return detail::shared<file>::make(client, std::move(res).value());
        else
            return std::unexpected(std::format("Failed to process file {} - {}\n", filename.string(), res.error()));
    }

    const nlohmann::json &json() const { return request; }
    
    file(secret, handle &client, nlohmann::json &&json)
        : request(std::move(json)), M_client(&client)
    {
    }

    // delete file from /v1/files
    ~file();
private:
    static std::expected<nlohmann::json, std::string> process(handle &client, std::span<const std::byte> bytes, const std::filesystem::path &filename);

    nlohmann::json request;
    handle *M_client;
};

AI_END