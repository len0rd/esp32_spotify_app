#ifndef ___HTTPCLIENT_HPP__
#define ___HTTPCLIENT_HPP__

#include <string>
#include <esp_http_client.h>
#include "esp_log.h"

#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class HttpClient
{
public:
    HttpClient();

    // Perform GET request and return parsed JSON
    json get(const std::string& url, const std::string& header_key = "",
             const std::string& header_value = "");

    json post(const std::string& url, const std::string& header_key = "",
              const std::string& header_value = "", std::string body = "",
              const std::string& user = "", const std::string& password = "",
              bool ignore_response = false);

    json put(const std::string& url, const std::string& header_key = "",
             const std::string& header_value = "", std::string body = "",
             bool ignore_response = false);

    void setLogLevel(esp_log_level_t level)
    {
        esp_log_level_set(TAG, level);
    }

private:
    const char* TAG = "HttpClient";

    static constexpr size_t BUFFER_SIZE = 10240;

    bool isJson(const std::string& str)
    {
        return (!str.empty() && (str[0] == '{' || str[0] == '[') &&
                (str.back() == '}' || str.back() == ']'));
    }
};

#endif // ___HTTPCLIENT_HPP__