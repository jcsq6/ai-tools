#pragma once
#include <expected>
#include <concepts>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

#include <print>

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <variant>

#include <json.hpp>

#define AI_BEG namespace ai {
#define AI_END }

AI_BEG

namespace detail
{
    template <typename T>
    class shared : public std::enable_shared_from_this<shared<T>>
    {
    public:
        using handle_t = std::shared_ptr<T>;

        template <typename Self>
        auto get_ptr(this Self &&self)
        {
            return std::dynamic_pointer_cast<std::remove_reference_t<Self>>(self.shared_from_this());
        }

        virtual ~shared() = default;
    protected:
        struct secret {};
        shared() = default;

        using parent = shared<T>;

        static auto make(auto &&...args)
        {
            return std::make_shared<T>(secret{}, std::forward<decltype(args)>(args)...);
        }
    };

    template <typename T>
    class owned
    {
    public:
        using handle_t = std::unique_ptr<T>;
    protected:
        struct secret {};
        owned() = default;

        using parent = owned<T>;

        static auto make(auto &&...args)
        {
            return std::make_unique<T>(secret{}, std::forward<decltype(args)>(args)...);
        }
    };
}

class handle : public detail::owned<handle>
{
public:
    handle(secret);
    static auto make(){ return parent::make(); }

    auto &key() const { return M_key; }
private:
    std::string M_key;
};

class assistant : public detail::shared<assistant>
{
public:
    template <std::ranges::range R = std::ranges::empty_view<std::string>> requires(std::convertible_to<std::ranges::range_value_t<R>, std::string_view>)
    assistant(secret, handle &client,
              std::string_view name,
              std::string_view instructions,
              std::string_view model,
              R &&tools = {},
              const nlohmann::json &response_format = {})
              : M_client(client), M_name(name), M_instructions(instructions), M_model(model), M_response_format(response_format), M_tools(tools | std::ranges::to<std::vector<std::string>>())
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
        return parent::make(client, name, instructions, model, std::forward<R>(tools), response_format);
    }

    auto &client() const { return M_client; }
    auto &name() const { return M_name; }
    auto &instructions() const { return M_instructions; }
    auto &model() const { return M_model; }
    auto &response_format() const { return M_response_format; }

private:
    handle &M_client;
    std::string M_name;
    std::string M_instructions;
    std::string M_model;
    nlohmann::json M_response_format;
    nlohmann::json M_request;
    std::vector<std::string> M_tools;

    friend class thread;
};

enum class severity_t
{
    info,
    warning,
    error,
    fatal
};

namespace detail
{
    class raw_stream
    {
    public:
        using error_fun_t = std::function<void(severity_t, std::string_view)>;

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
        error_fun_t error;
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

class stream_handler : public detail::shared<stream_handler>
{
public:
    virtual ~stream_handler() = default;

    using error_fun_t = detail::raw_stream::error_fun_t;

    auto &response_id() const { return M_stream.response_id; }
    auto &message_id() const { return M_stream.message_id; }
    auto &err() const { return M_stream.err; }
    auto &err_msg() const { return M_stream.err_msg; }
    auto created_at() const { return M_stream.created_at; }
    auto finished() const { return M_stream.finished; }

    void clear() { M_stream.clear(); }

    void set_error(error_fun_t error) { M_stream.error = std::move(error); }
protected:
    detail::raw_stream M_stream;

    friend class thread;
};

class tool;
class file;

class input_content
{
public:
    using array_value_t = std::variant<std::string, std::shared_ptr<file>>;
    using array_t = std::vector<array_value_t>;

    input_content(nlohmann::json) = delete;
    
    input_content(std::string_view message) : value(std::string(message))
    {
    }

    input_content(std::size_t capacity = 0) : value(array_t{})
    {
        std::get<array_t>(value).reserve(capacity);
    }

    input_content(std::initializer_list<array_value_t> list) : value(array_t{list})
    {
    }

    template <std::ranges::range R> requires(std::convertible_to<std::ranges::range_value_t<R>, array_value_t>)
    input_content(R && list) : value(list | std::ranges::to<array_t>())
    {
    }

    input_content(const input_content &content) = default;
    input_content(input_content &&content) = default;
    input_content &operator=(const input_content &content) = default;
    input_content &operator=(input_content &&content) = default;

    void append(std::string_view message)
    {
        if (!std::holds_alternative<array_t>(value))
            throw std::runtime_error("Input is not an array."); // use exceptions instead of std::expected for elegance here
        
        if (message.empty())
            return;

        auto &arr = std::get<array_t>(value);
        arr.push_back(std::string(message));
    }

    void append(std::shared_ptr<file> file)
    {
        if (!std::holds_alternative<array_t>(value))
            throw std::runtime_error("Input is not an array."); // use exceptions instead of std::expected for elegance here
        
        auto &arr = std::get<array_t>(value);
        arr.push_back(std::move(file));
    }

    template <std::ranges::range R> requires(std::convertible_to<std::ranges::range_value_t<R>, array_value_t>)
    void append(R && list)
    {
        if (!std::holds_alternative<array_t>(value))
            throw std::runtime_error("Input is not an array."); // use exceptions instead of std::expected for elegance here

        auto &arr = std::get<array_t>(value);
        arr.append_range(std::forward<R>(list));
    }

    void set(std::string_view message)
    {   
        value = std::string(message);
    }

    void set(std::initializer_list<array_value_t> list)
    {
        value = array_t{list};
    }

    template <std::ranges::range R> requires(std::convertible_to<std::ranges::range_value_t<R>, array_value_t>)
    void set(R && list)
    {
        value = list | std::ranges::to<array_t>();
    }
    
    auto files() const
    {
        static const array_t empty{};
        auto on_range = [](auto &&range) {
            return range |
                   std::views::filter([](const array_value_t &item) { return item.index() == 1; }) |
                   std::views::transform([](const array_value_t &item) { return std::get<std::shared_ptr<file>>(item); });
        };

        if (std::holds_alternative<array_t>(value))
            return on_range(std::get<array_t>(value));
        else
            return on_range(empty);
    }

    nlohmann::json json() const;

public:
    std::variant<std::string, array_t> value;
};

class input_t
{
public:
    enum class role
    {
        user,
        assistant,
        developer,
    };

    using array_value_t = std::pair<role, input_content>;
    using array_t = std::vector<array_value_t>;

    input_t(nlohmann::json) = delete;
    
    input_t(const std::string &message) : value(message)
    {
    }
    input_t(const char *message) : value(std::string(message))
    {
    }
    input_t(std::string_view message) : value(std::string(message))
    {
    }

    input_t(std::size_t capacity = 0) : value(array_t{})
    {
        std::get<array_t>(value).reserve(capacity);
    }

    input_t(std::initializer_list<array_value_t> list) : value(array_t{list})
    {
    }

    template <std::ranges::range R> requires(std::convertible_to<std::ranges::range_value_t<R>, array_value_t>)
    input_t(R && list) : value(list | std::ranges::to<array_t>())
    {
    }

    input_t(const input_t &content) = default;
    input_t(input_t &&content) = default;
    input_t &operator=(const input_t &content) = default;
    input_t &operator=(input_t &&content) = default;

    void append(role _role, input_content message)
    {
        if (!std::holds_alternative<array_t>(value))
            throw std::runtime_error("Input is not an array."); // use exceptions instead of std::expected for elegance here

        auto &arr = std::get<array_t>(value);
        arr.push_back({_role, std::move(message)});
    }

    auto files() const
    {
        static const array_t empty{};
        auto on_range = [](auto &&range) {
            return range |
                   std::views::transform([](const array_value_t &item) { return item.second.files(); }) |
                   std::views::join;
        };

        if (std::holds_alternative<array_t>(value))
            return on_range(std::get<array_t>(value));
        else
            return on_range(empty);
    }

    nlohmann::json json() const;

public:
    std::variant<std::string, array_t> value;
};

class thread : public detail::shared<thread>
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

    // TODO: allow continuing previous threads
    // thread(secret, std::string_view id);

    static auto make(assistant &assistant) { return parent::make(assistant); }

    auto &get_assistant() const { return *M_assistant; }
    auto &error() const { return M_err; }

    // takes shared ownership of attached files into the sending thread
    void send(const input_t &input, stream_handler &output);

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
    }

    bool is_running() const
    {
        return M_thread.joinable();
    }

private:
    std::vector<message> M_messages;
    assistant::handle_t M_assistant;

    std::jthread M_thread;
    std::exception_ptr M_err;

    static size_t sse_write(void *contents, size_t size, size_t nmemb, void *userp);
};

template <typename DeltaFn, typename FinishFn>
struct delta_funs
{
    DeltaFn delta = nullptr;
    FinishFn finish = nullptr;
    stream_handler::error_fun_t error = nullptr;
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
        M_stream.error = std::move(args.error);
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