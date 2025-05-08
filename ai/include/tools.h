#pragma once
#include "ai.h"
#include <optional>
#include <ranges>
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

namespace detail
{
    template <typename T>
    concept has_schema = requires { T::schema(); };
}

class tool
{    
public:
    auto start_thread() 
    {
        return thread::make(*M_assistant);
    }

    template <typename Self>
    std::expected<void, std::string> send(this Self &&self, thread &th, input &&in, stream_handler &res)
    {
        if (&th.get_assistant() != self.M_assistant.get())
            return std::unexpected("Thread does not belong to this assistant.");

        return self.send_impl(th, std::move(in), res);
    }

protected:
    assistant::handle_t M_assistant;

    template <typename Self>
    auto make_format(this Self &self)
    {
        if constexpr (detail::has_schema<Self>)
        {
            return nlohmann::json{
                {"format", {
                    {"type", "json_schema"},
                    {"name", Self::name()},
                    {"schema", Self::schema()}
                }}
            };
        }
        else
            return nlohmann::json{};
    }

    template <typename Self>
    void init(this Self &self, handle &client)
    {
        self.M_assistant = assistant::make(
            client,
            self.name(),
            self.instructions(),
            self.model(),
            self.tools(),
            self.make_format()
        );
    }
};

class reworder : public tool
{
public:
    reworder(handle &client)
    {
        init(client);
    }

    static constexpr std::string_view name() { return "Reworder"; }
    static inline std::string_view instructions() { return M_instructions; }
    static constexpr std::string_view model() { return "gpt-4.1"; }
    static const nlohmann::json &schema() { return M_schema; }
    static constexpr auto tools() { return std::views::empty<std::string>; }
    
private:
    static nlohmann::json M_schema;
    static std::string_view M_instructions;

    friend tool;

    std::expected<void, std::string> send_impl(thread &th, input &&in, stream_handler &res);
};

class ask : public tool
{
public:
    ask(handle &client)
    {
        init(client);
    }

    static constexpr std::string_view name() { return "Ask"; }
    static inline std::string_view instructions() { return M_instructions; }
    static constexpr std::string_view model() { return "gpt-4.1"; }
    static constexpr auto tools() { return std::views::single(std::string_view("web_search_preview")); }

private:
    static std::string_view M_instructions;

    friend tool;

    std::expected<void, std::string> send_impl(thread &th, input &&in, stream_handler &res);
};

AI_END