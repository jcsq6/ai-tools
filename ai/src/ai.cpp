#include "ai.h"

#include <cstdlib>

#include <print>
#include <iostream>
#include <utility>
#include <thread>
#include <fstream>

#include <curl/curl.h>

AI_BEG

handle::handle()
{    
    if (auto var = std::getenv("OPENAI_API_KEY"))
    {
        M_key = var;
        return;
    }

    std::ifstream file(".env");
    if (file)
    {
        while (std::getline(file, M_key))
        {
            std::string_view lv = M_key;
            if (auto loc = lv.rfind("OPENAI_API_KEY", 0); loc == 0)
            {
                lv.remove_prefix(14);
                while (std::isspace(lv.front()) || lv.front() == '=')
                    lv.remove_prefix(1);

                M_key = lv;
                return;
            }
        }
    }

    std::print(std::cerr, "No OpenAI API key found in environment variables or .env file.\n");
    std::exit(1);
}

handle::~handle()
{
}

assistant::assistant(handle &_client,
                     std::string_view name,
                     std::string_view instructions,
                     std::string_view model,
                     const nlohmann::json &response_format)
    : M_client(&_client), M_name(name), M_instructions(instructions), M_model(model), M_response_format(response_format)
{
    std::print("Initializing assistant {} with model {}...\n", name, model);

    M_request["stream"] = true;
    M_request["model"] = model;
    M_request["instructions"] = instructions;
    if (!response_format.empty())
        M_request["text"] = response_format;
    M_request["previous_response_id"] = nullptr;
    M_request["input"] = std::string();
}

std::string_view trimmed(std::string_view str)
{
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    if (str.empty())
        return str;
    str.remove_prefix((std::min)(str.find_first_not_of(whitespace), str.size()));
    str.remove_suffix((str.size() - 1) - str.find_last_not_of(whitespace));
    return str;
}

void stream_handler::parse()
{
    size_t pos = 0;

    M_blocks.clear();

    const std::string cbuff = M_buffer;

    // search for blank lind for end of SSE block
    while (true)
    {
        size_t end = cbuff.find("\n\n", pos);
        if (end == std::string::npos)
            break;

        M_blocks.push_back(std::string_view(cbuff).substr(pos, end - pos));
        pos = end + 2;
    }

    // remove what was parsed
    if (pos > 0)
        M_buffer.erase(0, pos);

    for (auto &block : M_blocks)
    {
        std::string_view event_name;
        std::string_view data;

        for (size_t line_start = 0; line_start < block.size();)
        {
            size_t newline_pos = block.find('\n', line_start);
            if (newline_pos == std::string::npos)
                newline_pos = block.size();
            
            std::string_view line = block.substr(line_start, newline_pos - line_start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            
            if (line.rfind("event:", 0) == 0)
                event_name = trimmed(line.substr(6));
            else if (line.rfind("data:", 0) == 0)
                data = trimmed(line.substr(5));
            
            line_start = newline_pos + 1;
        }

        if (event_name == "response.output_text.delta")
        {
            try
            {
                auto j = nlohmann::json::parse(data);
                if (j.contains("delta"))
                {
                    std::string delta = j["delta"];
                    M_accum.append(delta);
                    if (M_delta)
                        M_delta(M_accum, delta);
                }
            } catch (...)
            {
                std::print(std::cerr, "Failed to parse delta: {}\n", data);
            }
        }
        else if (event_name == "response.output_text.done")
        {
            try
            {
                auto j = nlohmann::json::parse(data);
                if (j.contains("text_id"))
                    M_message_id = j["item_id"];
            }
            catch(const std::exception& e)
            {
                std::print(std::cerr, "Failed to parse message id: {}\n", data);
            }

            if (M_finish)
                M_finish(M_accum);
            finished = true;
            return;
        }
        else if (event_name == "response.failed")
        {
            try
            {
                auto j = nlohmann::json::parse(data);
                if (j.contains("error"))
                {
                    M_err = j["error"]["code"];
                    M_err_msg = j["error"]["message"];

                    std::print(std::cerr, "Request failed with code {}: {}\n", M_err, M_err_msg);
                }
                finished = true;
            } catch (...)
            {
                std::print(std::cerr, "Failed to parse failure message: {}\n", data);
            }
            return;
        }
        else if (event_name == "response.created")
        {
            try
            {
                auto j = nlohmann::json::parse(data);
                if (j.contains("response"))
                {
                    M_response_id = j["response"]["id"];
                    M_created_at = j["response"]["created_at"];
                }
            } catch (...)
            {
                std::print(std::cerr, "Failed to parse response: {}\n", data);
            }
        }
    }
}

thread::thread(assistant &assistant) : M_assistant(&assistant)
{
    std::print("Initializing thread for assistant {}...\n", M_assistant->name());
}

size_t thread::sse_write(void *contents, size_t size, size_t nmemb, void *userp)
{
    stream_handler *res = static_cast<stream_handler *>(userp);
    const auto total_size = size * nmemb;
    res->M_buffer.append(static_cast<char *>(contents), total_size);
    if (!res->finished)
        res->parse();
    return total_size;
}

void thread::send(std::string_view input, stream_handler &res)
{
    if (M_thread.joinable())
        join();
    
    auto runner = [this, input, &res]()
    {
        res.reset();
        try
        {
            auto &request = M_assistant->M_request;
            request["input"] = std::string(input);
            if (!M_messages.empty())
                request["previous_response_id"] = M_messages.back().id;

            CURL *curl = curl_easy_init();
            if (!curl)
                throw std::runtime_error("Failed to initialize libcurl.\n");

            std::string_view key = M_assistant->client().key();
            constexpr std::string_view url = "https://api.openai.com/v1/responses";
            std::string body = request.dump();

            curl_easy_setopt(curl, CURLOPT_URL, url.data());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());

            curl_slist *headers = nullptr;
            auto header = std::format("Authorization: Bearer {}", key);
            headers = curl_slist_append(headers, header.data());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);

            curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

            CURLcode cres = curl_easy_perform(curl);
            if (cres != CURLE_OK)
                throw std::runtime_error(std::format("Request failed: {}", curl_easy_strerror(cres)));

            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

            if (response_code != 200)
            {
                try
                {
                    auto j = nlohmann::json::parse(res.M_buffer);
                    res.M_err = j["error"]["code"];
                    res.M_err_msg = j["error"]["message"];
                }
                catch(...)
                {
                }
                
                if (res.M_err.empty())
                    throw std::runtime_error(std::format("Request failed with status code: {}", response_code));
                else
                    throw std::runtime_error(std::format("Request failed with code {}: {}", res.M_err, res.M_err_msg));
            }

            M_messages.push_back({.id = res.M_response_id, .input = std::string(input), .response = res.M_accum, .created_at = res.M_created_at});
        } catch (...)
        {
            M_exception = std::current_exception();
        }
    };

    M_thread = std::jthread(runner);
}

AI_END