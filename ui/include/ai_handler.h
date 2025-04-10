#pragma once
#include "database.h"
#include "tools.h"

class ai_handler
{
public:
    static constexpr std::string_view database_dir = "database";
    ai_handler() :
        M_db(database_dir),
        M_handle(),
        M_reworder(M_handle)
    {
    }

    auto &reworder()
    {
        return M_reworder;
    }

    auto &database()
    {
        return M_db;
    }


private:
    ai::database M_db;
    ai::handle M_handle;
    ai::reworder M_reworder;
};