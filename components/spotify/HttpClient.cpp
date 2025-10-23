#include "HttpClient.hpp"

HttpClient::HttpClient() {}

// Perform GET request and return parsed JSON
json HttpClient::get(const std::string& url, const std::string& header_key,
                     const std::string& header_value)
{
    esp_http_client_config_t config = {};
    memset(&config, 0, sizeof(config));
    config.url                         = url.c_str();
    config.timeout_ms                  = 10000;
    config.skip_cert_common_name_check = true; // disable CN check

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (!header_key.empty() && !header_value.empty())
    {
        esp_http_client_set_header(client, header_key.c_str(), header_value.c_str());
    }

    char buffer[BUFFER_SIZE];
    int  total_len = 0;
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        total_len = esp_http_client_fetch_headers(client);
        if (total_len < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else
        {
            int data_read = esp_http_client_read_response(client, buffer, BUFFER_SIZE);
            if (data_read <= 0)
            {
                ESP_LOGE(TAG, "Failed to read response");
            }

            ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %" PRId64,
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
        }
    }
    esp_http_client_close(client);

    if (total_len <= 0)
    {
        ESP_LOGW(TAG, "No response body received");
        esp_http_client_cleanup(client);
        return {};
    }
    buffer[total_len] = '\0';
    esp_http_client_cleanup(client);

    if (!isJson(buffer))
    {
        ESP_LOGE(TAG, "Response is not valid JSON: %s", buffer);
        return {};
    }

    json j = json::parse(std::string(buffer, total_len), nullptr, false);
    if (j.is_discarded())
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", buffer);
        return {};
    }
    else
    {
        return j;
    }
}

json HttpClient::post(const std::string& url, const std::string& header_key,
                      const std::string& header_value, std::string body, const std::string& user,
                      const std::string& password, bool ignore_response)
{
    esp_http_client_config_t config = {};
    memset(&config, 0, sizeof(config));
    config.url                         = url.c_str();
    config.timeout_ms                  = 10000;
    config.skip_cert_common_name_check = true; // disable CN check

    char output_buffer[BUFFER_SIZE] = {0};
    int  content_length             = 0;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t                err    = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set HTTP method: %s", esp_err_to_name(err));
        return {};
    }

    if (!header_key.empty() && !header_value.empty())
    {
        err = esp_http_client_set_header(client, header_key.c_str(), header_value.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
            return {};
        }
    }
    else
    {
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
        // esp_http_client_set_header(client, "Accept", "application/json");
    }

    if (!user.empty())
    {
        err = esp_http_client_set_username(client, user.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP username: %s", esp_err_to_name(err));
            return {};
        }
        esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    }
    if (!password.empty())
    {
        err = esp_http_client_set_password(client, password.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP password: %s", esp_err_to_name(err));
            return {};
        }
    }

    err = esp_http_client_open(client, body.length());
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return {};
    }
    else
    {
        // send body
        if (!body.empty())
        {
            int wlen = esp_http_client_write(client, body.c_str(), body.length());
            if (wlen < 0)
            {
                ESP_LOGE(TAG, "Write failed");
                return {};
            }
        }

        // get response
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
            return {};
        }
        else
        {
            int data_read = esp_http_client_read_response(client, output_buffer, BUFFER_SIZE);
            if (data_read >= 0)
            {
                output_buffer[content_length] = '\0';
                ESP_LOGD(TAG, "HTTP POST Status = %d, content_length = %" PRId64,
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response");
                return {};
            }
        }
    }
    esp_http_client_cleanup(client);

    if (content_length == 0 && !ignore_response)
    {
        ESP_LOGW(TAG, "No response body received");
        return {};
    }
    if (!isJson(output_buffer))
    {
        if (ignore_response)
        {
            return {};
        }
        ESP_LOGE(TAG, "Response is not valid JSON: %s", output_buffer);
        return {};
    }

    json j = nlohmann::json::parse(std::string(output_buffer, content_length), nullptr, false);
    if (j.is_discarded())
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", output_buffer);
        return {};
    }
    else
    {
        return j;
    }
}

json HttpClient::put(const std::string& url, const std::string& header_key,
                     const std::string& header_value, std::string body, bool ignore_response)
{
    esp_http_client_config_t config = {};
    memset(&config, 0, sizeof(config));
    config.url                         = url.c_str();
    config.timeout_ms                  = 10000;
    config.skip_cert_common_name_check = true; // disable CN check

    char output_buffer[BUFFER_SIZE] = {0};
    int  content_length             = 0;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t                err    = esp_http_client_set_method(client, HTTP_METHOD_PUT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set HTTP method: %s", esp_err_to_name(err));
        return {};
    }

    if (!header_key.empty() && !header_value.empty())
    {
        err = esp_http_client_set_header(client, header_key.c_str(), header_value.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
            return {};
        }
    }

    if (!body.empty() && isJson(body))
    {
        err = esp_http_client_set_header(client, "Content-Type", "application/json");
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
            return {};
        }
    }

    err = esp_http_client_open(client, body.length());
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return {};
    }
    else
    {
        // send body
        if (!body.empty())
        {
            int wlen = esp_http_client_write(client, body.c_str(), body.length());
            if (wlen < 0)
            {
                ESP_LOGE(TAG, "Write failed");
                return {};
            }
        }

        // get response
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
            return {};
        }
        else
        {
            int data_read = esp_http_client_read_response(client, output_buffer, BUFFER_SIZE);
            if (data_read >= 0)
            {
                output_buffer[content_length] = '\0';
                ESP_LOGD(TAG, "HTTP PUT Status = %d, content_length = %" PRId64,
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response");
                return {};
            }
        }
    }
    esp_http_client_cleanup(client);

    if (content_length == 0 && !ignore_response)
    {
        ESP_LOGW(TAG, "No response body received");
        return {};
    }
    if (!isJson(output_buffer))
    {
        if (ignore_response)
        {
            return {};
        }
        ESP_LOGE(TAG, "Response is not valid JSON: %s", output_buffer);
        return {};
    }

    json j = nlohmann::json::parse(std::string(output_buffer, content_length), nullptr, false);
    if (j.is_discarded())
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", output_buffer);
        return {};
    }
    else
    {
        return j;
    }
}
