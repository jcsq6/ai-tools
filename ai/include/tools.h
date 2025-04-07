#pragma once
#include "ai.h"

AI_BEG

class tool
{
public:
    template <typename Self>
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
    static constexpr std::string_view instructions() { return "Improve the given text, rewording if necessary."; }
    static constexpr std::string_view model() { return "gpt-4o"; }
    static const nlohmann::json &schema() { return M_schema; }

private:
    static nlohmann::json M_schema;
};

AI_END