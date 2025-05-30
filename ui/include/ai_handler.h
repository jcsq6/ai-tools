#pragma once
#include "ai.h"
#include "database.h"
#include "tools.h"

class ai_handler
{
public:
    static constexpr std::string_view database_dir = "database";
    ai_handler() :
        M_db(database_dir),
        M_handle(ai::handle::make()),
        M_reworder(*M_handle),
        M_ask(*M_handle)
    {
    }

    auto &client()
    {
        return *M_handle;
    }

    auto &reworder()
    {
        return M_reworder;
    }

    auto &ask()
    {
        return M_ask;
    }

    auto &database()
    {
        return M_db;
    }


private:
    ai::database M_db;
    ai::handle::handle_t M_handle;
    ai::reworder M_reworder;
    ai::ask M_ask;
};