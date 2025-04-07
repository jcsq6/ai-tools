#include "ai.h"
#include "database.h"
#include <print>
#include <iostream>

void text_test(ai::handle &client)
{
    auto res = ai::text_stream_handler::make({
        .delta = [](std::string_view accum, std::string_view delta) { std::print("{}", delta); },
        .finish = [](std::string_view accum) { std::print("\n"); }
    });
    ai::assistant assistant(client, "test", "You have no purpose outside of API endpoint testing", "gpt-4o-mini");
    ai::thread thread(assistant);
    thread.send("Testing!", res);
    thread.send("What did I just say?", res);

    ai::database db("database", false);
    db.append(thread);
}

void json_test(ai::handle &client)
{
    auto res = ai::json_stream_handler::make({
        .delta = [](const nlohmann::json &accum) { std::print("{}\n\n", accum.dump(4)); }
    });
    ai::assistant assistant(client,
        "test",
        "You have no purpose outside of API endpoint testing",
        "gpt-4o-mini",
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
    ai::thread thread(assistant);
    // thread.send("Testing!", res);
    thread.send("Give me something to shatter my json parser?", res);

    ai::database db("database", false);
    db.append(thread);
}

int main(int argc, char *argv[])
{
    try
    {
        ai::handle client;
        if (argc > 1 && std::string_view(argv[1]) == "json")
            json_test(client);
        else
            text_test(client);

        // auto tests = {
        //     // std::string_view(R"({"response":"Try this: {\")"),
        //     // std::string_view(R"({"response":"Try this: {\"key)"),
        //     // std::string_view(R"({"response":"Try this: {\"key1)"),
        //     // std::string_view(R"({"response":"Try this: {\"key1\": \"value1\", \"key2")"),
        //     // std::string_view(R"({"response":"Try this: {\"key1\": \"value1\", \"key2\")"),
        //     std::string_view(R"({")"),
        //     std::string_view(R"({"explanation":"Improved the phrasing to sound more natural and confident.",")")
        // };

        // auto res = ai::json_stream_handler::make({});
        // for (auto test : tests)
        // {
        //     std::print("Testing: {}\n", test);
        //     res->parse(test);
        //     std::print("{}\n\n", res->accum().dump(4));
        // }

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