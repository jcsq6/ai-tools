#include "ai.h"
#include "database.h"
#include "tools.h"
#include <print>
#include <iostream>
#include <fstream>

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

        constexpr std::string_view filename = "assets/tool_test.jpg";
        std::ifstream file(filename.data(), std::ios::binary);
        auto image = std::ranges::subrange{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}} | std::ranges::to<std::vector>();
        
        auto text = "This is a paragraph that is not written very well. It has a lot of issues, like grammar problems and unclear ideas, and it doesn't really make sense. I think it could be improved a lot if someone could help make it better and easier to understand.";
        reworder.send(thread, text, "", image, res);

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