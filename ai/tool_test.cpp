#include "ai.h"
#include "database.h"
#include "tools.h"
#include <print>
#include <iostream>

int main(int argc, char *argv[])
{
    try
    {
        ai::handle client;
        ai::reworder reworder(client);

        auto thread = reworder.start_thread();
        auto res = ai::json_stream_handler::make({
            .delta = [](const nlohmann::json &accum) {
                std::print("\033[2J\033[H");
                std::print("Explanation: {}\n", accum.contains("explanation") && accum["explanation"].is_string() ? accum["explanation"].get<std::string>() : "");
                std::print("Improved: {}\n", accum.contains("improved") && accum["improved"].is_string() ? accum["improved"].get<std::string>() : "");
            }
        });

        thread.send("I'm not too good at talkin.", res);

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