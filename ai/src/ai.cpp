#include "ai.h"

#include <cstdlib>

#include <exception>
#include <print>
#include <iostream>
#include <utility>
#include <thread>
#include <fstream>

#include <curl/curl.h>

AI_BEG

handle::handle(handle::secret)
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
    throw std::runtime_error("No OpenAI API key found.");
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

void detail::raw_stream::parse(std::string_view delta_str)
{
    if (delta_str.empty())
        return;
    if (finished)
        return;
    
    buffer.append(delta_str);

    size_t pos = 0;

    M_blocks.clear();

    const std::string cbuff = buffer;

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
        buffer.erase(0, pos);

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
                    std::string ds = j["delta"];
                    accum.append(ds);
                    if (delta)
                        delta(accum, ds);
                }
            } catch (const std::exception& e)
            {
                error(severity_t::warning, std::format("Failed to parse delta - {}: {}", e.what(), data));
            }
        }
        else if (event_name == "response.output_text.done")
        {
            try
            {
                auto j = nlohmann::json::parse(data);
                if (j.contains("text_id"))
                    message_id = j["item_id"];
            }
            catch(const std::exception& e)
            {
                error(severity_t::warning, std::format("Failed to parse message id - {}: {}", e.what(), data));
            }

            if (finish)
                finish(accum);
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
                    err = j["error"]["code"];
                    err_msg = j["error"]["message"];

                    error(severity_t::error, std::format("Request failed with code {} - {}", err, err_msg));
                }
                finished = true;
            } catch (...)
            {
                error(severity_t::error, std::format("Failed to parse failure message - {}", data));
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
                    response_id = j["response"]["id"];
                    created_at = j["response"]["created_at"];
                }
            } catch (...)
            {
                error(severity_t::warning, std::format("Failed to parse response id - {}", data));
            }
        }
    }
}

size_t thread::sse_write(void *contents, size_t size, size_t nmemb, void *userp)
{
    const auto total_size = size * nmemb;
    static_cast<stream_handler *>(userp)->M_stream.parse(std::string_view(static_cast<char *>(contents), total_size));
    return total_size;
}

void thread::send(nlohmann::json input, stream_handler &output)
{
    if (!input.is_array() && !input.is_string())
    {
        std::print(std::cerr, "Input must be an array or string.\n");
        return;
    }

    auto handle = get_ptr();

    auto runner = [input = std::move(input), &output, handle = std::move(handle)]()
    {
        auto res = output.get_ptr();
        res->clear();
        try
        {
            auto &request = handle->M_assistant->M_request;
            request["input"] = std::move(input);
            if (!handle->M_messages.empty())
                request["previous_response_id"] = handle->M_messages.back().id;

            CURL *curl = curl_easy_init();
            if (!curl)
                throw std::runtime_error("Failed to initialize libcurl.\n");

            std::string_view key = handle->M_assistant->client().key();
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
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, res.get());

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
                    auto j = nlohmann::json::parse(res->M_stream.buffer);
                    res->M_stream.err = j["error"]["code"];
                    res->M_stream.err_msg = j["error"]["message"];
                }
                catch(...)
                {
                }
                
                if (res->M_stream.err.empty())
                    throw std::runtime_error(std::format("Request failed with status code: {}", response_code));
                else
                    throw std::runtime_error(std::format("Request failed with code {}: {}", res->M_stream.err, res->M_stream.err_msg));
            }
            
            auto text_input = [&request]() {
                auto &input = request["input"];
                if (input.is_string())
                    return input.get<std::string>();

                for (const auto &item : input)
                    if (item.contains("content"))
                        return item["content"] | std::views::filter([](auto &&content) { return content.contains("text"); }) |
                                                 std::views::transform([](auto &&content) { return std::format("\n{}", content["text"].template get<std::string>()); }) |
                                                 std::views::join | 
                                                 std::views::drop(1) | 
                                                 std::ranges::to<std::string>();

                return std::string{};
            }();

            handle->M_messages.push_back({.id = res->M_stream.response_id, .input = text_input, .response = res->M_stream.accum, .created_at = res->M_stream.created_at});
        }
        catch (const std::exception &e)
        {
            res->M_stream.error(severity_t::error, std::format("Error sending request - {}", e.what()));
            handle->M_err = std::current_exception();
        }
        catch (...)
        {
            res->M_stream.error(severity_t::error, std::format("Error sending request - Unknown error occurred."));
            handle->M_err = std::current_exception();
        }
    };

    M_thread = std::jthread(runner);
}

void json_stream_handler::parse(std::string_view accum)
{
    if (accum.empty())
        return;
    
    std::string modified(accum);
    std::vector<char> stack;
    stack.reserve(modified.size() / 2);

    constexpr std::string_view open = "[{\"";
    constexpr std::string_view close = "]}\"";

    bool escaped = false;

    for (auto c : modified)
    {
        if (c == '\\')
        {
            escaped = !escaped;
            continue;
        }

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (!stack.empty() && stack.back() == '"')
        {
            if (c == '"')
                stack.pop_back();
            continue;
        }

        if (open.find(c) != std::string_view::npos)
            stack.push_back(c);
        else if (close.find(c) != std::string_view::npos)
            stack.pop_back();
    }

    bool empty = false;
    // unfinished string
    if (stack.back() == '"')
    {
        stack.pop_back();
        empty = modified.back() == '"';
        modified += '"';
    }

    // unfinished dict
    if (stack.back() == '{')
    {
        // search for "
        auto loc = modified.rend();
        for (auto it = modified.rbegin(); it != modified.rend(); ++it)
            if (!std::isspace(*it))
            {
                if (*it == '"')
                    loc = it;
                break;
            }


        // search for the beginning of the string
        for (auto it = loc + 1; it < modified.rend();)
        {
            escaped = false;
            auto it2 = it + 1;
            for (; it2 < modified.rend(); ++it2)
                if (*it2 == '\\')
                    escaped = !escaped;
                else
                    break;

            if (!escaped && *it == '"')
            {
                loc = it;
                break;
            }

            it = it2;
        }
        
        // search for a colon
        for (auto it = loc + 1; it < modified.rend(); ++it)
        {
            if (!std::isspace(*it))
            {
                if (*it != ':') // key
                {
                    if (empty)
                        modified.erase(loc.base() - 1, modified.end());
                    else
                        modified += ": null";
                }
                break;
            }
        }

        for (auto it = modified.rbegin(); it < modified.rend(); ++it)
            if (!std::isspace(*it))
            {
                if (*it == ',')
                    modified.erase(it.base() - 1, modified.end());
                break;
            }
    }

    for (auto c : std::views::reverse(stack))
        modified += close[open.find(c)];

    M_accum = nlohmann::json::parse(modified);
}

AI_END