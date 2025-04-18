#include "tools.h"
#include <cppcodec/base64_rfc4648.hpp>

std::string to_base64(std::span<const std::byte> data)
{
    return cppcodec::base64_rfc4648::encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

AI_BEG
nlohmann::json reworder::M_schema = {
    {"type", "object"},
    {"properties", {
        {"improved", {
            {"type", "string"},
            {"description", "The improved text"}
        }},
        {"explanation", {
            {"type", "string"},
            {"description", "The explanation of the changes made"}
        }}
    }},
    {"required", {
        "improved",
        "explanation",
    }},
    {"additionalProperties", false},
};

void reworder::send(thread &th, std::string_view selected, std::span<const std::byte> image, std::shared_ptr<stream_handler> res)
{
    if (&th.get_assistant() != &M_assistant)
    {
        std::print(std::cerr, "Thread does not belong to this assistant.\n");
        return;
    }

    auto content = nlohmann::json::array({
        {
            {"type", "input_text"},
            {"text", selected}
        }
    });

    if (!image.empty())
        content.push_back({
            {"type", "input_image"},
            {"image_url", std::format("data:image/jpeg;base64,{}", to_base64(image))}
        });

    auto input = nlohmann::json::array({
        {
            {"role", "user"},
            {"content", std::move(content)}
        }
    });
    
    th.send(std::move(input), res);
}
AI_END