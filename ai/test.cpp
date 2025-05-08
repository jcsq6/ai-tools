#include "ai.h"
#include "database.h"
#include <print>
#include <iostream>
#include <ranges>

void print_error(ai::severity_t severity, std::string_view message)
{
    std::println(std::cerr, "{}: {}", severity == ai::severity_t::error ? "Error" : "Warning", message);
}

void text_test(ai::handle &client)
{
    auto res = ai::text_stream_handler::make({
        .delta = [](std::string_view accum, std::string_view delta) { std::print("{}", delta); },
        .finish = [](std::string_view accum) { std::print("\n"); },
        .error = print_error
    });
    auto assistant = ai::assistant::make(client, "test", "You have no purpose outside of API endpoint testing", "gpt-4o-mini");
    auto thread = ai::thread::make(*assistant);
    thread->send("Testing!", *res);
    thread->send("What did I just say?", *res);

    ai::database db("database", false);
    db.append(*thread);
}

void json_test(ai::handle &client)
{
    auto res = ai::json_stream_handler::make({
        .delta = [](const nlohmann::json &accum) { std::print("{}\n\n", accum.dump(4)); },
        .error = print_error
    });
    auto assistant = ai::assistant::make(client,
        "test",
        "You have no purpose outside of API endpoint testing",
        "gpt-4o-mini",
        {},
        {{"format", {
            {"type", "json_schema"},
            {"name", "test"},
            {"schema", {
                {"type", "object"},
                {"properties", {
                    {"test_type", {
                        {"type", "string"},
                        {"description", "The type of test"},
                    }},
                    {"response", {
                        {"type", "string"},
                        {"description", "The quippy response to the test"}
                    }}
                }},
                {"required", {
                    "test_type",
                    "response"
                }},
                {"additionalProperties", false},
        }}
    }}});
    auto thread = ai::thread::make(*assistant);
    // thread.send("Testing!", res);
    thread->send("Give me something to shatter my json parser?", *res);

    ai::database db("database", false);
    db.append(*thread);
}

template <std::ranges::range R>
void conversation(ai::handle &client, R &&tools)
{
    auto res = ai::text_stream_handler::make({
        .delta = [](std::string_view accum, std::string_view delta) { std::print("{}", delta); std::cout.flush(); },
        .finish = [](std::string_view accum) { std::print("\n"); },
        .error = print_error
    });
    auto assistant = ai::assistant::make(
        client, "test", "You have no purpose outside of API endpoint testing",
        "gpt-4o-mini", tools | std::ranges::to<std::vector<std::string>>());

    std::print("Tools: {}\n", tools);
    auto thread = ai::thread::make(*assistant);
    // thread.send("Testing!", res);
    while (true)
    {
        std::print("> ");
        std::string input;
        std::getline(std::cin, input);
        if (input.empty())
            break;

        std::print("Response:\n");
        thread->send(input, *res);
        thread->join();
    }

    ai::database db("database", false);
    db.append(*thread);
}

int main(int argc, char *argv[])
{
    try
    {
        auto args = std::ranges::subrange(argv + 1, argv + argc) | std::views::transform([](auto &&arg) { return std::string_view(arg); });
        auto client = ai::handle::make();
        if (std::ranges::contains(args, "--json"))
            json_test(*client);
        else if (std::ranges::contains(args, "--conversation"))
            conversation(*client, args | std::views::filter([](auto &&arg) { return arg != "--conversation"; }));
        else
            text_test(*client);

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