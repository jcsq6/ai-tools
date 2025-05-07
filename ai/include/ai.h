#pragma once

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
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

namespace detail
{
    template <typename T>
    class enable_shared : public std::enable_shared_from_this<enable_shared<T>>
    {
    public:
        using handle_t = std::shared_ptr<T>;

        template <typename Self>
        auto get_ptr(this Self &&self)
        {
            return std::dynamic_pointer_cast<std::remove_reference_t<Self>>(self.shared_from_this());
        }

        virtual ~enable_shared() = default;
    protected:
        struct secret {};
        enable_shared() = default;
    };
}

class handle : public detail::enable_shared<handle>
{
public:
    handle(secret);
    static auto make()
    {
        return std::make_shared<handle>(secret{});
    }

    auto &key() const { return M_key; }
private:
    std::string M_key;
};

class assistant : public detail::enable_shared<assistant>
{
public:
    template <std::ranges::range R = std::ranges::empty_view<std::string>> requires(std::convertible_to<std::ranges::range_value_t<R>, std::string_view>)
    assistant(secret, handle &client,
              std::string_view name,
              std::string_view instructions,
              std::string_view model,
              R &&tools = {},
              const nlohmann::json &response_format = {})
              : M_client(client.get_ptr()), M_name(name), M_instructions(instructions), M_model(model), M_response_format(response_format), M_tools(tools | std::ranges::to<std::vector<std::string>>())
    {
        M_request["stream"] = true;
        M_request["model"] = model;
        M_request["instructions"] = instructions;
        if (!response_format.empty())
            M_request["text"] = response_format;
        M_request["previous_response_id"] = nullptr;
        for (auto &tool : M_tools)
            M_request["tools"].push_back({{"type", tool}});
    }

    template <std::ranges::range R = std::ranges::empty_view<std::string>> requires(std::convertible_to<std::ranges::range_value_t<R>, std::string_view>)
    static auto make(handle &client,
                     std::string_view name,
                     std::string_view instructions,
                     std::string_view model,
                     R &&tools = {},
                     const nlohmann::json &response_format = {})
    {
        return std::make_shared<assistant>(secret{}, client, name, instructions, model, std::forward<R>(tools), response_format);
    }

    auto &client() const { return *M_client; }
    auto &name() const { return M_name; }
    auto &instructions() const { return M_instructions; }
    auto &model() const { return M_model; }
    auto &response_format() const { return M_response_format; }

private:
    handle::handle_t M_client;
    std::string M_name;
    std::string M_instructions;
    std::string M_model;
    nlohmann::json M_response_format;
    nlohmann::json M_request;
    std::vector<std::string> M_tools;

    friend class thread;
};

namespace detail
{
    class raw_stream
    {
    public:
        // delta: void(accum, delta)
        // finish: void(accum)
        raw_stream()
        {
            M_blocks.reserve(8);
        }

        void clear()
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
}

class stream_handler : public detail::enable_shared<stream_handler>
{
public:
    virtual ~stream_handler() = default;

    auto &response_id() const { return M_stream.response_id; }
    auto &message_id() const { return M_stream.message_id; }
    auto &err() const { return M_stream.err; }
    auto &err_msg() const { return M_stream.err_msg; }
    auto created_at() const { return M_stream.created_at; }
    auto finished() const { return M_stream.finished; }

    void clear() { M_stream.clear(); }
protected:
    detail::raw_stream M_stream;

    friend class thread;
};

class tool;

class thread : public detail::enable_shared<thread>
{
public:
    struct message
    {
        std::string id;
        std::string input;
        std::string response;
        std::time_t created_at;
    };

    using handle_t = std::shared_ptr<thread>;

    thread(secret, assistant &assistant) : M_assistant(assistant.get_ptr()) {}
    static auto make(assistant &assistant) { return std::make_shared<thread>(secret{}, assistant); }

    auto &get_assistant() const { return *M_assistant; }

    void send(nlohmann::json input, stream_handler &output);

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
    assistant::handle_t M_assistant;

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
public:
    using delta_fun_t = std::function<void(std::string_view accum, std::string_view delta)>;
    using finish_fun_t = std::function<void(std::string_view)>;
    using constructor_arg_t = delta_funs<delta_fun_t, finish_fun_t>;
    using handle_t = std::shared_ptr<text_stream_handler>;

    text_stream_handler(secret, constructor_arg_t &&args)
    {
        M_stream.delta = std::move(args.delta);
        M_stream.finish = std::move(args.finish);
    }

    static auto make(constructor_arg_t &&args)
    {
        return std::make_shared<text_stream_handler>(secret{}, std::move(args));
    }

    void set_delta(delta_fun_t delta) { M_stream.delta = std::move(delta); }
    void set_finish(finish_fun_t finish) { M_stream.finish = std::move(finish); }
};

class json_stream_handler : public stream_handler
{
public:
    using delta_fun_t = std::function<void(const nlohmann::json &)>;
    using finish_fun_t = std::function<void(const nlohmann::json &)>;
    using constructor_arg_t = delta_funs<delta_fun_t, finish_fun_t>;
    using handle_t = std::shared_ptr<json_stream_handler>;

    json_stream_handler(secret, constructor_arg_t &&args)
    {
        set_delta(std::move(args.delta));
        set_finish(std::move(args.finish));
    }

    static auto make(constructor_arg_t &&args)
    {
        return std::make_shared<json_stream_handler>(secret{}, std::move(args));
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