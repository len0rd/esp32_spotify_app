#include <SpotifyClient.hpp>

bool SpotifyClient::refreshToken()
{
    auto response_ptr =
        httpClient.post(AUTH_URL, "Content-Type", "application/x-www-form-urlencoded",
                        "grant_type=refresh_token&refresh_token=" + getRefreshToken(),
                        getClientId(), getClientSecret());
    if (response_ptr)
    {
        json& response = *response_ptr;

        if (response.contains("access_token"))
        {
            user1_access_token.set(response["access_token"]);
            return true;
        }
    }
    if (response_ptr && !response_ptr->is_discarded())
    {
        ESP_LOGI(TAG, "Token Refresh Response: %s", response_ptr->dump(2).c_str());
    }
    return false;
}

std::unique_ptr<json> SpotifyClient::get(const std::string& api_path)
{
    if (user1_access_token.get().empty())
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return nullptr;
        }
    }

    auto ret_ptr = httpClient.get(BASE_URL + api_path, "Authorization", "Bearer " + getToken());
    if (ret_ptr && ret_ptr->contains("error"))
    {
        json& ret = *ret_ptr;
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ESP_LOGI(TAG, "Token refreshed, retrying GET request...");
                    return httpClient.get(BASE_URL + api_path, "Authorization",
                                          "Bearer " + getToken());
                }
            }
            else
            {
                ESP_LOGE(TAG, "Error message: %s",
                         ret["error"]["message"].get<std::string>().c_str());
            }
        }
    }
    if (ret_ptr && !ret_ptr->is_discarded())
    {
        ESP_LOGI(TAG, "GET Response: %s", ret_ptr->dump(2).c_str());
    }
    return ret_ptr;
}

std::unique_ptr<json> SpotifyClient::post(const std::string& api_path, std::string body,
                                          bool ignore_response)
{
    if (user1_access_token.get().empty())
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return nullptr;
        }
    }

    auto ret_ptr = httpClient.post(BASE_URL + api_path, "Authorization", "Bearer " + getToken(),
                                   body, "", "", ignore_response);
    if (ret_ptr && ret_ptr->contains("error"))
    {
        json& ret = *ret_ptr;
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ESP_LOGI(TAG, "Token refreshed, retrying POST request...");
                    return httpClient.post(BASE_URL + api_path, "Authorization",
                                           "Bearer " + getToken(), body);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Error message: %s",
                         ret["error"]["message"].get<std::string>().c_str());
            }
        }
    }
    if (ret_ptr && !ret_ptr->is_discarded())
    {
        ESP_LOGI(TAG, "POST Response: %s", ret_ptr->dump(2).c_str());
    }
    return ret_ptr;
}

std::unique_ptr<json> SpotifyClient::put(const std::string& api_path, std::string body,
                                         bool ignore_response)
{
    if (user1_access_token.get().empty())
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return nullptr;
        }
    }

    auto ret_ptr = httpClient.put(BASE_URL + api_path, "Authorization", "Bearer " + getToken(),
                                  body, ignore_response);
    if (ret_ptr && ret_ptr->contains("error"))
    {
        json& ret = *ret_ptr;
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ESP_LOGI(TAG, "Token refreshed, retrying PUT request...");
                    return httpClient.put(BASE_URL + api_path, "Authorization",
                                          "Bearer " + getToken(), body);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Error message: %s",
                         ret["error"]["message"].get<std::string>().c_str());
            }
        }
    }
    if (ret_ptr && !ret_ptr->is_discarded())
    {
        ESP_LOGI(TAG, "PUT Response: %s", ret_ptr->dump(2).c_str());
    }
    return ret_ptr;
}