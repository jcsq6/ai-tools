#pragma once

#define AI_BEG namespace ai {
#define AI_END }

#include <string>
#include <vector>
#include <thread>
#include <exception>
#include <functional>

#include "json.hpp"

AI_BEG
class client
{
public:
    client();
    ~client();

    auto &key() const { return M_key; }
private:
    std::string M_key;
};

class assistant
{
public:
    assistant(client &_client,
              std::string_view name,
              std::string_view instructions,
              std::string_view model,
              const nlohmann::json &response_format = {});
    ~assistant();

    auto &client() const { return *M_client; }
    auto &name() const { return M_name; }
    auto &instructions() const { return M_instructions; }
    auto &model() const { return M_model; }
    auto &response_format() const { return M_response_format; }

private:
    ai::client *M_client;
    std::string M_name;
    std::string M_instructions;
    std::string M_model;
    nlohmann::json M_response_format;
    nlohmann::json M_request;

    friend class thread;
};

class stream_handler
{
public:
    // delta: void(accum, delta)
    // finish: void(accum)
    stream_handler()
    {
        M_blocks.reserve(8);
    }

    struct constructor_args
    {
        std::function<void(std::string_view, std::string_view)> delta = nullptr;
        std::function<void(std::string_view)> finish = nullptr;
    };

    stream_handler(constructor_args &&args) : M_delta(std::move(args.delta)), M_finish(std::move(args.finish))
    {
        M_blocks.reserve(8);
    }

    void reset()
    {
        M_accum.clear();
        buffer.clear();
        M_id.clear();
        M_err.clear();
        M_err_msg.clear();
        M_blocks.clear();
        finished = false;
    }
private:
    std::function<void(std::string_view, std::string_view)> M_delta;
    std::function<void(std::string_view)> M_finish;
    std::string M_accum;
    std::string buffer;
    std::string M_id;
    std::vector<std::string_view> M_blocks; // optimization
    std::string M_err;
    std::string M_err_msg;
    bool finished = false;


    friend class thread;
    void parse();
};

class thread
{
public:
    thread(assistant &assistant);

    void send(std::string_view input, stream_handler &res);
    const auto &get_message_ids() const { return M_ids; }

    void join()
    {
        if (M_thread.joinable())
            M_thread.join();
        if (M_exception)
            std::rethrow_exception(M_exception);
    }

    ~thread()
    {
        join();
    }
private:
    std::vector<std::string> M_ids;
    assistant *M_assistant;

    std::jthread M_thread;
    std::exception_ptr M_exception = nullptr;

    static size_t sse_write(void *contents, size_t size, size_t nmemb, void *userp);
};
AI_END