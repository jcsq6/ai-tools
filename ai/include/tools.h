#pragma once
#include "ai.h"
#include <concepts>
#include <optional>
#include <span>
#include <expected>
#include <optional>

AI_BEG

struct input
{
    std::optional<std::string_view> selected = std::nullopt;
    std::optional<std::string_view> prompt = std::nullopt;

    struct images_struct
    {
        static constexpr std::size_t max_images = 4;

        template <std::ranges::contiguous_range... Rs> requires(sizeof...(Rs) <= max_images)
        images_struct(Rs &&...imgs) : 
            images{}, count{}
        {
            auto add_image = [this](auto &&img) {
                if (!std::ranges::empty(img))
                    images[count++] = std::as_bytes(std::span(img));
            };
            (add_image(std::forward<Rs>(imgs)), ...);
        }

        auto get() const
        {
            return std::span(images).subspan(0, count);
        }
        
    private:
        std::array<std::span<const std::byte>, max_images> images;
        std::size_t count = 0;
    };

    using imgs_t = images_struct;

    std::optional<images_struct> images;
};

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

    virtual std::expected<void, std::string> send(thread &th, input &&in, std::shared_ptr<stream_handler> res) = 0;

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

    std::expected<void, std::string> send(thread &th, input &&in, std::shared_ptr<stream_handler> res) override;
    
private:
    static nlohmann::json M_schema;
    static std::string_view M_instructions;
};

class ask : public tool
{
public:
    ask(handle &client) : tool(*this, client) {}

    static constexpr std::string_view name() { return "Ask"; }
    static inline std::string_view instructions() { return M_instructions; }
    static constexpr std::string_view model() { return "gpt-4.1"; }
    static const nlohmann::json &schema() { return M_schema; }

    std::expected<void, std::string> send(thread &th, input &&in, std::shared_ptr<stream_handler> res) override;

private:
    static nlohmann::json M_schema;
    static std::string_view M_instructions;
};

AI_END