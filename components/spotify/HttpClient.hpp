/**
 * @file HttpClient.hpp
 * @brief HTTP client wrapper for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the HttpClient class which provides a simplified interface
 * for making HTTP requests (GET, POST, PUT) on ESP32 devices. It handles JSON
 * parsing and provides basic authentication support using the ESP-IDF HTTP client.
 */

#ifndef ___HTTPCLIENT_HPP__
#define ___HTTPCLIENT_HPP__

#include <string>
#include <esp_http_client.h>
#include "esp_log.h"

#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @class HttpClient
 * @brief HTTP client wrapper for ESP32 applications
 *
 * This class provides a simplified interface for making HTTP requests using
 * the ESP-IDF HTTP client. It automatically handles JSON parsing of responses
 * and supports basic authentication. The class is designed to work with
 * REST APIs and provides methods for GET, POST, and PUT requests.
 */
class HttpClient
{
public:
    /**
     * @brief Constructor for HttpClient
     */
    HttpClient();

    /**
     * @brief Perform a GET request and return parsed JSON response
     * @param url The complete URL to send the GET request to
     * @param header_key Optional custom header key (default: empty)
     * @param header_value Optional custom header value (default: empty)
     * @return JSON object containing the parsed response, or empty JSON on error
     */
    json get(const std::string& url, const std::string& header_key = "",
             const std::string& header_value = "");

    /**
     * @brief Perform a POST request and return parsed JSON response
     * @param url The complete URL to send the POST request to
     * @param header_key Optional custom header key (default: empty)
     * @param header_value Optional custom header value (default: empty)
     * @param body Request body content (default: empty)
     * @param user Username for basic authentication (default: empty)
     * @param password Password for basic authentication (default: empty)
     * @param ignore_response If true, don't parse response as JSON (default: false)
     * @return JSON object containing the parsed response, or empty JSON if ignore_response is true or on error
     */
    json post(const std::string& url, const std::string& header_key = "",
              const std::string& header_value = "", std::string body = "",
              const std::string& user = "", const std::string& password = "",
              bool ignore_response = false);

    /**
     * @brief Perform a PUT request and return parsed JSON response
     * @param url The complete URL to send the PUT request to
     * @param header_key Optional custom header key (default: empty)
     * @param header_value Optional custom header value (default: empty)
     * @param body Request body content (default: empty)
     * @param ignore_response If true, don't parse response as JSON (default: false)
     * @return JSON object containing the parsed response, or empty JSON if ignore_response is true or on error
     */
    json put(const std::string& url, const std::string& header_key = "",
             const std::string& header_value = "", std::string body = "",
             bool ignore_response = false);

    /**
     * @brief Set the logging level for HttpClient operations
     * @param level ESP-IDF log level to set for this component
     */
    void setLogLevel(esp_log_level_t level)
    {
        esp_log_level_set(TAG, level);
    }

private:
    const char* TAG = "HttpClient"; ///< Log tag for ESP-IDF logging

    static constexpr size_t BUFFER_SIZE = 10240; ///< Buffer size for HTTP response data

    json performRequest(esp_http_client_method_t method, const std::string& url,
                        const std::string& header_key, const std::string& header_value,
                        const std::string& body, const std::string& user,
                        const std::string& password, bool ignore_response);

    /**
     * @brief Check if a string contains valid JSON format
     * @param str String to check for JSON format
     * @return true if string appears to be valid JSON (starts with { or [ and ends with } or ]), false otherwise
     */
    bool isJson(const std::string& str);
};

#endif // ___HTTPCLIENT_HPP__