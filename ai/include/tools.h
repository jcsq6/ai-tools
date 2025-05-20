#pragma once
#include "ai.h"
#include "file.h"
#include <initializer_list>
#include <ranges>
#include <expected>
#include <type_traits>

AI_BEG

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

    // will check files for validity
    template <typename Self, std::ranges::range R> requires(std::same_as<std::ranges::range_value_t<std::remove_cvref_t<R>>, file::handle_t>)
    std::expected<void, std::string> initial_send(this Self &&self, thread &th, stream_handler &res, R &&files, std::string_view prompt = {}, std::string_view selected = {})
    {
        if (&th.get_assistant() != self.M_assistant.get())
            return std::unexpected("Thread does not belong to this assistant.");

        if (th.get_messages().size() > 0)
            return std::unexpected("Thread already has messages.");

        return self.send_impl(th, res, std::forward<R>(files), prompt, selected);
    }

    // will check files for validity
    template <typename Self, std::ranges::range R> requires(std::same_as<std::ranges::range_value_t<std::remove_cvref_t<R>>, file::handle_t>)
    std::expected<void, std::string> send(this Self &&self, thread &th, stream_handler &res, R &&files, std::string_view prompt)
    {
        if (&th.get_assistant() != self.M_assistant.get())
            return std::unexpected("Thread does not belong to this assistant.");

        if (th.get_messages().size() == 0)
            return std::unexpected("Thread has no messages.");

        return self.send_impl(th, res, std::forward<R>(files), prompt, {});
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

    template <std::ranges::range R>
    std::expected<void, std::string> send_impl(thread &th, stream_handler &res, R &&files, std::string_view prompt, std::string_view selected)
    {
        if (selected.empty() && prompt.empty())
            return std::unexpected("No selected text or prompt provided.");

        // TODO: use XML input (https://platform.openai.com/docs/guides/text?api-mode=responses)
        nlohmann::json input_text = {};
        if (selected.empty())
            input_text["Selected"] = selected;
        if (prompt.empty())
            input_text["Prompt"] = prompt;

        input_content content(1 + std::ranges::size(files));
        content.append(input_text.dump(2));
        content.append(files | std::views::filter([](auto &&file) { return bool(file); }));

        input_t input(1);
        input.append(input_t::role::user, std::move(content));

        th.send(input, res);

        return {};
    }
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

    template <std::ranges::range R>
    std::expected<void, std::string> send_impl(thread &th, stream_handler &res, R &&files, std::string_view prompt, std::string_view selected)
    {
        if (prompt.empty())
            return std::unexpected("No prompt provided.");

        // TODO: use XML input (https://platform.openai.com/docs/guides/text?api-mode=responses)
        nlohmann::json input_text = {{"Prompt", prompt}};
        if (!selected.empty())
            input_text["Selected"] = selected;

        input_content content(1 + std::ranges::size(files));
        content.append(input_text.dump(2));
        content.append(files | std::views::filter([](auto &&file) { return bool(file); }));

        input_t input(1);
        input.append(input_t::role::user, std::move(content));

        th.send(input, res);

        return {};
    }
};

AI_END