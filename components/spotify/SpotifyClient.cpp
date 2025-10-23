#include <SpotifyClient.hpp>

bool SpotifyClient::refreshToken()
{
    json response = httpClient.post(AUTH_URL, "Content-Type", "application/x-www-form-urlencoded",
                                    "grant_type=refresh_token&refresh_token=" + getRefreshToken(),
                                    getClientId(), getClientSecret());
    if (response.contains("access_token"))
    {
        user1_access_token.set(params::password(response["access_token"].get<std::string>()));
        return true;
    }
    return false;
}

json SpotifyClient::get(const std::string& api_path)
{
    if (getToken().size() == 0)
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return {};
        }
    }

    json ret = httpClient.get(BASE_URL + api_path, "Authorization", "Bearer " + getToken());
    if (ret.contains("error"))
    {
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ret.clear();
                    ESP_LOGI(TAG, "Token refreshed, retrying GET request...");
                    ret = httpClient.get(BASE_URL + api_path, "Authorization",
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
    ESP_LOGD(TAG, "GET Response: %s", ret.dump(4).c_str());
    return ret;
}

json SpotifyClient::post(const std::string& api_path, std::string body, bool ignore_response)
{
    if (getToken().size() == 0)
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return {};
        }
    }

    json ret = httpClient.post(BASE_URL + api_path, "Authorization", "Bearer " + getToken(), body,
                               "", "", ignore_response);
    if (ret.contains("error"))
    {
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ret.clear();
                    ESP_LOGI(TAG, "Token refreshed, retrying POST request...");
                    ret = httpClient.post(BASE_URL + api_path, "Authorization",
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
    ESP_LOGD(TAG, "POST Response: %s", ret.dump(4).c_str());
    return ret;
}

json SpotifyClient::put(const std::string& api_path, std::string body, bool ignore_response)
{
    if (getToken().size() == 0)
    {
        ESP_LOGI(TAG, "No access token, refreshing token...");
        if (!refreshToken())
        {
            ESP_LOGE(TAG, "Failed to refresh token");
            return {};
        }
    }

    json ret = httpClient.put(BASE_URL + api_path, "Authorization", "Bearer " + getToken(), body,
                              ignore_response);
    if (ret.contains("error"))
    {
        if (ret["error"].contains("message"))
        {
            if (ret["error"]["message"] == "Invalid access token" ||
                ret["error"]["message"] == "The access token expired" ||
                ret["error"]["message"] == "Only valid bearer authentication supported")
            {
                ESP_LOGI(TAG, "Access token expired, refreshing token...");
                if (refreshToken())
                {
                    ret.clear();
                    ESP_LOGI(TAG, "Token refreshed, retrying PUT request...");
                    ret = httpClient.put(BASE_URL + api_path, "Authorization",
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
    ESP_LOGD(TAG, "PUT Response: %s", ret.dump(4).c_str());
    return ret;
}