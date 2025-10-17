#ifndef __SPOTIFYCLIENT_HPP__
#define __SPOTIFYCLIENT_HPP__

#include <HttpClient.hpp>
#include <Param.h>
#include <vector>
#include <string>

class SpotifyClient
{
private:
    SpotifyClient() {}
    ~SpotifyClient() {}

public:
    static SpotifyClient& getInstance()
    {
        static SpotifyClient instance;
        return instance;
    }

    bool refreshToken();
    json get(const std::string& api_path);
    json post(const std::string& api_path, std::string body = "", bool ignore_response = false);
    json put(const std::string& api_path, std::string body = "", bool ignore_response = false);

    void setLogLevel(esp_log_level_t level)
    {
        esp_log_level_set(TAG, level);
        httpClient.setLogLevel(level);
    }

private:
    const char*       TAG      = "SpotifyClient";
    const std::string BASE_URL = "https://api.spotify.com/v1/";
    const std::string AUTH_URL = "https://accounts.spotify.com/api/token";

    HttpClient httpClient;

    const std::string getClientId() const
    {
        return SPOTIFY_CLIENT_ID;
    }
    const std::string getClientSecret() const
    {
        return SPOTIFY_CLIENT_SECRET;
    }
    const std::string getRefreshToken()
    {
        return user1_refresh_token.get();
    }
    const std::string getToken()
    {
        return user1_access_token.get();
    }

    const std::string SPOTIFY_CLIENT_ID     = "9aebd5c245f3425e82263b91dced28c9";
    const std::string SPOTIFY_CLIENT_SECRET = "2f3d8c07eb06420da2b8214857599971";

    params::Param<std::string> user1_name          = {"user1_name", ""};
    params::Param<std::string> user1_access_token  = {"user1_token", ""};
    params::Param<std::string> user1_refresh_token = {"user1_refresh", ""};
};

#endif // __SPOTIFYCLIENT_HPP__