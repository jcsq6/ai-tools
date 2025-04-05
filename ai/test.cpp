#include "ai.h"
#include "database.h"
#include <print>
#include <iostream>

int main()
{
    try
    {
        ai::handle client;
        ai::stream_handler res({
            .delta = [](std::string_view accum, std::string_view delta) { std::print("{}", delta); },
            .finish = [](std::string_view accum) { std::print("\n"); }
        });
        ai::assistant assistant(client, "test", "You have no purpose outside of API endpoint testing", "gpt-4o-mini");
        ai::thread thread(assistant);
        thread.send("Testing!", res);
        thread.send("What did I just say?", res);

        ai::database db("database", false);
        db.append(thread);
        return 0;
    }
    catch(const std::exception &e)
    {
        std::print(std::cerr, "Failure: {}\n", e.what());
        return 1;
    }
    catch (...)
    {
        std::print(std::cerr, "Unknown failure\n");
        return 1;
    }
}