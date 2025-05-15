#include "ai.h"
#include "database.h"
#include "tools.h"
#include <print>
#include <iostream>
#include <string_view>

std::expected<ai::thread::handle_t, std::string> reword(ai::handle &client)
{
    ai::reworder reworder(client);

    auto thread = reworder.start_thread();
    auto res = ai::json_stream_handler::make({
        .delta = [](const nlohmann::json &accum) {
            std::print("\033[2J\033[H");
            std::print("Explanation: {}\n", accum.contains("explanation") && accum["explanation"].is_string() ? accum["explanation"].get<std::string>() : "");
            std::print("Improved: {}\n", accum.contains("improved") && accum["improved"].is_string() ? accum["improved"].get<std::string>() : "");
        },
        .error = [](ai::severity_t severity, std::string_view message) {
            std::println(std::cerr, "{}: {}", severity == ai::severity_t::error ? "Error" : "Warning", message);
        }
    });

    auto text = "This is a paragraph that is not written very well. It has a lot of issues, like grammar problems and unclear ideas, and it doesn't really make sense. I think it could be improved a lot if someone could help make it better and easier to understand.";
    if (auto err = reworder.send(*thread, {
        .selected = text,
        .prompt = "Please improve the text.",
        .files = {ai::file_view("assets/tool_test.jpg")}
    }, *res); !err)
        return std::unexpected(std::format("Failed to send request: {}", err.error()));
    
    return thread;
}

std::expected<ai::thread::handle_t, std::string> ask(ai::handle &client)
{
    ai::ask ask(client);

    auto thread = ask.start_thread();
    auto res = ai::text_stream_handler::make({
        .delta = [](std::string_view accum, std::string_view delta) {
            std::print("{}", delta);
            std::cout.flush();
        },
        .finish = [](std::string_view) { std::print("\n"); },
        .error = [](ai::severity_t severity, std::string_view message) {
            std::println(std::cerr, "{}: {}", severity == ai::severity_t::error ? "Error" : "Warning", message);
        }
    });
    
    if (auto err = ask.send(*thread, {
        .prompt = "How do I change the theme? What are some good modern themes to use? Use the internet to find themes.",
        .files = {ai::file_view("assets/tool_test.jpg")}
    }, *res); !err)
        return std::unexpected(std::format("Failed to send request: {}", err.error()));
    
    return thread;
}

int main(int argc, char *argv[])
{
    try
    {
        auto args = std::ranges::subrange(argv + 1, argv + argc) | std::views::transform([](auto &&arg) { return std::string_view(arg); });
        
        auto client = ai::handle::make();
        
        std::expected<ai::thread::handle_t, std::string> res;
        if (std::ranges::contains(args, "--reword"))
            res = reword(*client);
        else if (std::ranges::contains(args, "--ask"))
            res = ask(*client);
        else
            res = std::unexpected("No valid tool specified. Use --reword or --ask.");

        if (!res)
        {
            std::print(std::cerr, "{}\n", res.error());
            return 1;
        }

        ai::database db("database", false);
        if (auto r = db.append(**res); !r)
            std::print(std::cerr, "Failed to append thread: {}\n", r.error());

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