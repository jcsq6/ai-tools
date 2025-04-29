#pragma once
#include "ai.h"
#include <__expected/expected.h>
#include <concepts>
#include <span>
#include <expected>

AI_BEG

class tool
{
public:
    template <std::derived_from<tool> Self>
    tool(Self &self, handle &client) :
        M_assistant(
            client,
            Self::name(),
            Self::instructions(),
            Self::model(),
            make_format<Self>()
        )
    {
    }

    thread start_thread() 
    {
        return thread(M_assistant);
    }

protected:
    assistant M_assistant;

    template <typename Self>
    static auto make_format()
    {
        return nlohmann::json{
            {"format", {
                {"type", "json_schema"},
                {"name", Self::name()},
                {"schema", Self::schema()}
            }}
        };
    }
};


class reworder : public tool
{
public:
    reworder(handle &client) : tool(*this, client) {}

    static constexpr std::string_view name() { return "Reworder"; }
    static inline std::string_view instructions() { return M_instructions; }
    static constexpr std::string_view model() { return "gpt-4.1"; }
    static const nlohmann::json &schema() { return M_schema; }

    static std::string format(std::string_view response)
    {
        try
        {
            auto j = nlohmann::json::parse(response);
            auto improved = j["improved"].get<std::string>();
            auto explanation = j["explanation"].get<std::string>();
            return std::format("<html>"
                               "<body>"
                               "<p style='font-weight: bold; font-size: 12px; color: black;'>Improved Text:</p>"
                               "<p style='font-size:11px; color: black;'>{}</p>"
                               "<p style='font-weight: bold; font-size: 12px; color: black;'>Explanation:</p>"
                               "<p style='font-size:11px; color: black;'>{}</p>"
                               "</body>"
                               "</html>",
                               improved, explanation);
        }
        catch(const std::exception& e)
        {
            std::print(std::cerr, "Error parsing response: {}\n", e.what());
            return std::format("<html>"
                               "<body>"
                               "<p style='font-size:11px; color: black;'>{}</p>"
                               "</body>"
                               "</html>",
                               response);
        }
    }

    template <std::ranges::contiguous_range R>
    std::expected<void, std::string> send(thread &th, std::string_view selected, std::string_view prompt, R &&jpeg_image, std::shared_ptr<stream_handler> res)
    {
        return send_impl(th, selected, prompt, std::as_bytes(std::span(jpeg_image)), std::move(res));
    }

private:
    static nlohmann::json M_schema;
    static std::string_view M_instructions;

    std::expected<void, std::string> send_impl(thread &th, std::string_view selected, std::string_view prompt, std::span<const std::byte> image, std::shared_ptr<stream_handler> res);
};

AI_END