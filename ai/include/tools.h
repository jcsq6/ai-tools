#pragma once
#include "ai.h"

AI_BEG
class tool
{
public:
    tool(handle &_client);
    virtual ~tool() = default;

    virtual std::string_view name() const = 0;
    virtual std::string_view description() const = 0;

protected:
    virtual std::string_view instructions() const = 0;
    virtual std::string_view model() const = 0;
    virtual const nlohmann::json &response_format() const = 0;

    
};

AI_END