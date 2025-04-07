#include "tools.h"

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
AI_END