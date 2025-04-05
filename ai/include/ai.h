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
class handle
{
public:
    handle();
    ~handle();

    auto &key() const { return M_key; }
private:
    std::string M_key;
};

class assistant
{
public:
    assistant(handle &_client,
              std::string_view name,
              std::string_view instructions,
              std::string_view model,
              const nlohmann::json &response_format = {});

    auto &client() const { return *M_client; }
    auto &name() const { return M_name; }
    auto &instructions() const { return M_instructions; }
    auto &model() const { return M_model; }
    auto &response_format() const { return M_response_format; }

private:
    handle *M_client;
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
        M_buffer.clear();
        M_response_id.clear();
        M_message_id.clear();
        M_err.clear();
        M_err_msg.clear();
        M_blocks.clear();
        finished = false;
    }

    auto &accum() const { return M_accum; }
    auto &response_id() const { return M_response_id; }
    auto &message_id() const { return M_message_id; }
    auto &err() const { return M_err; }
    auto &err_msg() const { return M_err_msg; }
private:
    std::function<void(std::string_view, std::string_view)> M_delta;
    std::function<void(std::string_view)> M_finish;
    std::string M_accum;
    std::string M_buffer;
    std::string M_response_id;
    std::string M_message_id;
    std::vector<std::string_view> M_blocks; // optimization
    std::string M_err;
    std::string M_err_msg;
    std::time_t M_created_at = 0;
    bool finished = false;


    friend class thread;
    void parse();
};

class thread
{
public:
    struct message
    {
        std::string id;
        std::string input;
        std::string response;
        std::time_t created_at;
    };

    thread(assistant &assistant);

    auto &get_assistant() const { return *M_assistant; }

    void send(std::string_view input, stream_handler &res);
    const auto &get_messages() const { return M_messages; }

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
    std::vector<message> M_messages;
    assistant *M_assistant;

    std::jthread M_thread;
    std::exception_ptr M_exception = nullptr;

    static size_t sse_write(void *contents, size_t size, size_t nmemb, void *userp);
};

AI_END