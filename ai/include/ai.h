#pragma once

#define AI_BEG namespace ai {
#define AI_END }

#include <print>

#include <string>
#include <vector>
#include <thread>
#include <exception>
#include <functional>

#include <json.hpp>

AI_BEG
class handle
{
public:
    handle();

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
              std::vector<std::string> tools = {},
              const nlohmann::json &response_format = {})
              : M_client(&_client), M_name(name), M_instructions(instructions), M_model(model), M_response_format(response_format), M_tools(std::move(tools))
    {
        std::print("Initializing assistant {} with model {}...\n", name, model);

        M_request["stream"] = true;
        M_request["model"] = model;
        M_request["instructions"] = instructions;
        if (!response_format.empty())
            M_request["text"] = response_format;
        M_request["previous_response_id"] = nullptr;
        for (auto &tool : M_tools)
            M_request["tools"].push_back({{"type", tool}});
    }

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
    std::vector<std::string> M_tools;

    friend class thread;
};

class raw_stream
{
public:
    // delta: void(accum, delta)
    // finish: void(accum)
    raw_stream()
    {
        M_blocks.reserve(8);
    }

    void reset()
    {
        accum.clear();
        buffer.clear();
        response_id.clear();
        message_id.clear();
        err.clear();
        err_msg.clear();
        M_blocks.clear();
        finished = false;
    }

    std::function<void(std::string_view, std::string_view)> delta;
    std::function<void(std::string_view)> finish;
    std::string accum;
    std::string buffer;
    std::string response_id;
    std::string message_id;
    std::string err;
    std::string err_msg;
    std::time_t created_at = 0;
    bool finished = false;

    void parse(std::string_view delta_str);
private:
    std::vector<std::string_view> M_blocks; // optimization
};

class stream_handler
{
public:
    stream_handler() = default;
    virtual ~stream_handler() = default;

    auto &response_id() const { return M_stream.response_id; }
    auto &message_id() const { return M_stream.message_id; }
    auto &err() const { return M_stream.err; }
    auto &err_msg() const { return M_stream.err_msg; }
    auto created_at() const { return M_stream.created_at; }
    auto finished() const { return M_stream.finished; }

    void reset() { M_stream.reset(); }
protected:
    raw_stream M_stream;
    friend class thread;
};

class tool;

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

    void send(nlohmann::json input, std::shared_ptr<stream_handler> res);

    template <std::derived_from<tool> Tool>
    void send(Tool &tool, auto &&... args)
    {
        tool.send(*this, std::forward<decltype(args)>(args)...);
    }

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

    std::thread M_thread;
    std::exception_ptr M_exception = nullptr;

    static size_t sse_write(void *contents, size_t size, size_t nmemb, void *userp);
};

template <typename DeltaFn, typename FinishFn>
struct delta_funs
{
    DeltaFn delta = nullptr;
    FinishFn finish = nullptr;
};

class text_stream_handler : public stream_handler
{
    struct secret {};
public:
    using delta_fun_t = std::function<void(std::string_view accum, std::string_view delta)>;
    using finish_fun_t = std::function<void(std::string_view)>;
    using constructor_arg_t = delta_funs<delta_fun_t, finish_fun_t>;

    text_stream_handler(constructor_arg_t &&args, secret)
    {
        M_stream.delta = std::move(args.delta);
        M_stream.finish = std::move(args.finish);
    }

    static auto make(constructor_arg_t &&args)
    {
        return std::make_shared<text_stream_handler>(std::move(args), secret{});
    }

    void set_delta(delta_fun_t delta) { M_stream.delta = std::move(delta); }
    void set_finish(finish_fun_t finish) { M_stream.finish = std::move(finish); }
};

class json_stream_handler : public stream_handler
{
    struct secret {};
public:

    using delta_fun_t = std::function<void(const nlohmann::json &)>;
    using finish_fun_t = std::function<void(const nlohmann::json &)>;

    using constructor_arg_t = delta_funs<delta_fun_t, finish_fun_t>;

    json_stream_handler(constructor_arg_t &&args, secret)
    {
        set_delta(std::move(args.delta));
        set_finish(std::move(args.finish));
    }

    static auto make(constructor_arg_t &&args)
    {
        return std::make_shared<json_stream_handler>(std::move(args), secret{});
    }

    void set_delta(delta_fun_t _delta)
    {
        if (_delta)
            M_stream.delta = [this, _delta = std::move(_delta)](std::string_view accum, std::string_view) {
                parse(accum);
                _delta(M_accum);
            };
        else
            M_stream.delta = nullptr;
    }
    
    void set_finish(finish_fun_t finish)
    {
        if (finish)
            M_stream.finish = [this, finish = std::move(finish)](std::string_view accum) {
                parse(accum);
                finish(M_accum);
            };
        else
            M_stream.finish = nullptr;
    }

    void parse(std::string_view accum);

    const auto &accum() const { return M_accum; }
private:
    nlohmann::json M_accum;
};

AI_END