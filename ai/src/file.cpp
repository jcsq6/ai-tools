#include "file.h"

#include <expected>
#include <print>
#include <string>

#include <cppcodec/base64_rfc4648.hpp>

#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>

#include <curl/curl.h>

AI_BEG

std::string to_base64(std::span<const std::byte> data)
{
    return cppcodec::base64_rfc4648::encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::expected<std::string, std::string> get_text(std::span<const std::byte> data)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UCharsetDetector, decltype(&ucsdet_close)> detector(ucsdet_open(&status), &ucsdet_close);
    if (U_FAILURE(status))
        return std::unexpected("Failed to create charset detector.");

    ucsdet_setText(detector.get(), reinterpret_cast<const char*>(data.data()), data.size(), &status);
    if (U_FAILURE(status))
        return std::unexpected("Failed to set text for charset detection.");

    int32_t num = 0;
    auto matches = ucsdet_detectAll(detector.get(), &num, &status);
    if (U_FAILURE(status) || num == 0)
        return std::unexpected("No encodings found.");

    const char *detected = ucsdet_getName(matches[0], &status);
    std::print("Using encoding: {}\n", detected);

    int32_t src_len = data.size();
    int32_t dest_len = src_len * 4 + 1;
    std::string res;
    res.resize(dest_len);

    int32_t actual_len = ucnv_convert(
        "UTF-8",
        detected,
        res.data(),
        dest_len,
        reinterpret_cast<const char*>(data.data()),
        src_len,
        &status
    );
    if (U_FAILURE(status))
        return std::unexpected("Failed to convert encoding.");
    res.resize(actual_len);

    return res;
}

std::expected<std::string, std::string> upload_file(handle &client, const std::filesystem::path &filename, std::span<const std::byte> data)
{
    static const auto g = []() { curl_global_init(CURL_GLOBAL_ALL); return true; }(); // TODO: make global and safe

    CURL *curl = curl_easy_init();
    if (!curl)
        return std::unexpected("Failed to initialize libcurl.");

    curl_mime *mime = curl_mime_init(curl);

    // -F file=@filename
    curl_mimepart *file_part = curl_mime_addpart(mime);
    curl_mime_name(file_part, "file");
    auto name_storage = filename.filename().string();
    curl_mime_filename(file_part, name_storage.c_str());
    curl_mime_type(file_part, "application/octet-stream");
    curl_mime_data(file_part, reinterpret_cast<const char *>(data.data()), data.size());

    // -F purpose="assistants"
    curl_mimepart *purpose_part = curl_mime_addpart(mime);
    curl_mime_name(purpose_part, "purpose");
    curl_mime_data(purpose_part, "assistants", CURL_ZERO_TERMINATED);

    // request
    auto auth_storage = std::format("Authorization: Bearer {}", client.key());
    curl_slist *headers = curl_slist_append(nullptr, auth_storage.c_str());
    // headers = curl_slist_append(headers, "Expect:");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/files");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void *contents, size_t size, size_t nmemb, void *userp) -> std::size_t {
        static_cast<std::string *>(userp)->append(static_cast<const char *>(contents), size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return std::unexpected(std::format("Failed to upload file: {}", curl_easy_strerror(res)));

    try
    {
        auto json = nlohmann::json::parse(response);
        return json["id"];
    }
    catch (const nlohmann::json::parse_error &e)
    {
        return std::unexpected(std::format("Failed to parse response: {}", e.what()));
    }
    catch (...)
    {
        return std::unexpected("Failed to parse response.");
    }
}

std::expected<nlohmann::json, std::string> file::process(handle &client, std::span<const std::byte> bytes, const std::filesystem::path &filename)
{
    using namespace std::literals;
    constexpr auto images = std::array{".jpg"sv, ".jpeg"sv, ".png"sv, ".gif"sv, ".webp"sv};

    auto ext = filename.extension();
    if (std::ranges::contains(images, ext))
    {
        if (auto res = upload_file(client, filename, bytes))
            return nlohmann::json{
                {"type", "input_image"},
                {"file_id", *res}
            };
        else
            return std::unexpected(std::format("Failed to upload image file {} - {}\n", filename.string(), res.error()));
    }
    else if (ext == ".pdf")
    {
        if (auto res = upload_file(client, filename, bytes))
            return nlohmann::json{
                {"type", "input_file"},
                {"file_id", *res}
            };
        else
            return std::unexpected(std::format("Failed to upload PDF file {} - {}\n", filename.string(), res.error()));
    }
    else
    {
        // TODO: convert to PDF, then upload that (I'm never doing this)
        // The vector store doesn't work for this purpose
        if (auto text = get_text(bytes))
            return nlohmann::json{
                {"type", "input_text"},
                {"text", std::format("**Contents of file \"{}\"**:\n{}", filename.string(), *text)}
            };
        else
            return std::unexpected(std::format("Failed to detect text encoding for file {} - {}\n", filename.string(), text.error()));
    }
}

AI_END