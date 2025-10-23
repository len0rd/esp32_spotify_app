/**
 * @file SpotifyClient.hpp
 * @brief Low-level HTTP client for Spotify Web API communication
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the SpotifyClient class which handles direct HTTP
 * communication with the Spotify Web API, including authentication,
 * token management, and API request execution.
 */

#ifndef __SPOTIFYCLIENT_HPP__
#define __SPOTIFYCLIENT_HPP__

#include <HttpClient.hpp>
#include <Param.h>
#include <vector>
#include <string>

/**
 * @class SpotifyClient
 * @brief Low-level HTTP client for Spotify Web API communication
 *
 * This singleton class provides direct HTTP access to the Spotify Web API.
 * It handles OAuth2 token refresh, API authentication, and raw HTTP requests
 * (GET, POST, PUT) to Spotify endpoints. This class is typically used by
 * higher-level wrapper classes like the Spotify class.
 */
class SpotifyClient
{
private:
    /**
     * @brief Private constructor for singleton pattern
     */
    SpotifyClient() {}

    /**
     * @brief Private destructor for singleton pattern
     */
    ~SpotifyClient() {}

public:
    /**
     * @brief Get the singleton instance of the SpotifyClient class
     * @return Reference to the singleton SpotifyClient instance
     */
    static SpotifyClient& getInstance()
    {
        static SpotifyClient instance;
        return instance;
    }

    /**
     * @brief Refresh the Spotify API access token using the stored refresh token
     * @return true if token refresh was successful, false otherwise
     */
    bool refreshToken();

    /**
     * @brief Perform a GET request to a Spotify API endpoint
     * @param api_path The API endpoint path (relative to base URL)
     * @return JSON response from the API
     */
    json get(const std::string& api_path);

    /**
     * @brief Perform a POST request to a Spotify API endpoint
     * @param api_path The API endpoint path (relative to base URL)
     * @param body Request body content (optional)
     * @param ignore_response If true, don't parse response as JSON (optional, default: false)
     * @return JSON response from the API (empty if ignore_response is true)
     */
    json post(const std::string& api_path, std::string body = "", bool ignore_response = false);

    /**
     * @brief Perform a PUT request to a Spotify API endpoint
     * @param api_path The API endpoint path (relative to base URL)
     * @param body Request body content (optional)
     * @param ignore_response If true, don't parse response as JSON (optional, default: false)
     * @return JSON response from the API (empty if ignore_response is true)
     */
    json put(const std::string& api_path, std::string body = "", bool ignore_response = false);

    /**
     * @brief Set the logging level for SpotifyClient operations
     * @param level ESP-IDF log level to set for this component and HTTP client
     */
    void setLogLevel(esp_log_level_t level)
    {
        esp_log_level_set(TAG, level);
        httpClient.setLogLevel(level);
    }

private:
    const char*       TAG      = "SpotifyClient";               ///< Log tag for ESP-IDF logging
    const std::string BASE_URL = "https://api.spotify.com/v1/"; ///< Base URL for Spotify Web API
    const std::string AUTH_URL =
        "https://accounts.spotify.com/api/token"; ///< URL for token authentication

    HttpClient httpClient; ///< HTTP client instance for making requests

    /**
     * @brief Get the Spotify application client ID
     * @return Spotify client ID string
     */
    const std::string getClientId()
    {
        return SPOTIFY_CLIENT_ID.get();
    }

    /**
     * @brief Get the Spotify application client secret
     * @return Spotify client secret string
     */
    const std::string getClientSecret()
    {
        return SPOTIFY_CLIENT_SECRET.get();
    }

    /**
     * @brief Get the stored refresh token for the current user
     * @return Stored refresh token string
     */
    const std::string getRefreshToken()
    {
        return user1_refresh_token.get();
    }

    /**
     * @brief Get the current access token for API requests
     * @return Current access token string
     */
    std::string getToken()
    {
        return user1_access_token.get();
    }

    ///< Stored Spotify application client ID
    params::Param<std::string> SPOTIFY_CLIENT_ID = {"SPOTIFY_CLIENT_ID", ""};
    ///< Stored Spotify application client secret key
    params::Param<params::password> SPOTIFY_CLIENT_SECRET = {"SPOTIFY_CLIENT_SECRET",
                                                             params::password("")};
    ///< Stored username for user 1
    params::Param<std::string> user1_name = {"user1_name", ""};
    ///< Stored access token for user 1
    params::Param<params::password> user1_access_token = {"user1_token", params::password("")};
    ///< Stored refresh token for user 1
    params::Param<params::password> user1_refresh_token = {"user1_refresh", params::password("")};
};

#endif // __SPOTIFYCLIENT_HPP__