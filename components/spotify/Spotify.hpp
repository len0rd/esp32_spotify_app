/**
 * @file Spotify.hpp
 * @brief Spotify Web API wrapper class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify class which provides a high-level interface
 * for interacting with the Spotify Web API on ESP32 devices. It handles
 * playback control, track information, and user interaction with Spotify.
 */

#ifndef __SPOTIFY_HPP__
#define __SPOTIFY_HPP__

#include <SpotifyClient.hpp>
#include <vector>

using json = nlohmann::json;

/**
 * @class Spotify
 * @brief High-level wrapper for Spotify Web API functionality
 *
 * This singleton class provides a simplified interface for controlling Spotify
 * playback, retrieving track information, and managing playback state on ESP32
 * devices. It abstracts the complexity of the underlying SpotifyClient and
 * provides structured data types for common Spotify operations.
 */
class Spotify
{
private:
    /**
     * @brief Private constructor for singleton pattern
     */
    Spotify();

    /**
     * @brief Private destructor for singleton pattern
     */
    ~Spotify();

public:
    /**
     * @struct TrackInfo
     * @brief Contains information about a Spotify track
     */
    struct TrackInfo
    {
        std::string name;        ///< Track name
        std::string artist;      ///< Primary artist name
        std::string album;       ///< Album name
        std::string track_uri;   ///< Spotify URI for the track
        int         duration_ms; ///< Track duration in milliseconds

        /**
         * @brief Convert track information to string representation
         * @return String containing formatted track information
         */
        std::string toString();
    };
    /**
     * @struct CurrentlyPlayingInfo
     * @brief Contains information about the currently playing track and playback progress
     */
    struct CurrentlyPlayingInfo
    {
        TrackInfo   currentTrack; ///< Information about the currently playing track
        std::string context_uri;  ///< URI of the context (playlist, album, etc.) being played
        int         progress_ms;  ///< Playback progress in milliseconds at timestamp
        uint64_t    timestamp_ms; ///< Timestamp when progress was last updated

        /**
         * @brief Get current playback progress accounting for elapsed time
         * @return Current progress in milliseconds
         */
        int getProgress_ms();

        /**
         * @brief Get current playback progress accounting for elapsed time
         * @return Current progress in milliseconds
         */
        int getRemaining_ms();

        /**
         * @brief Get current playback progress as percentage
         * @return Progress as percentage (0-100)
         */
        float getProgress_percent();

        /**
         * @brief Convert currently playing info to string representation
         * @return String containing formatted playback information
         */
        std::string toString();
    };
    /**
     * @struct PlaybackState
     * @brief Contains information about the current playback state and device
     */
    struct PlaybackState
    {
        bool        is_playing;      ///< Whether playback is currently active
        bool        shuffle_state;   ///< Whether shuffle mode is enabled
        std::string repeat_state;    ///< Repeat mode ("off", "context", "track")
        int         volume_percent;  ///< Current volume level (0-100)
        bool        supports_volume; ///< Whether the device supports volume control
        std::string device_name;     ///< Name of the active playback device
        std::string device_id;       ///< ID of the active playback device

        /**
         * @brief Convert playback state to string representation
         * @return String containing formatted playback state information
         */
        std::string toString();
    };

    /**
     * @brief Get the singleton instance of the Spotify class
     * @return Reference to the singleton Spotify instance
     */
    static Spotify& getInstance()
    {
        static Spotify instance;
        return instance;
    }

    /**
     * @brief Check if music is currently playing
     * @return true if music is playing, false otherwise
     */
    bool isPlaying();

    /**
     * @brief Start or resume playback
     * @return true if successful, false otherwise
     */
    bool play();

    /**
     * @brief Pause playback
     * @return true if successful, false otherwise
     */
    bool pause();

    /**
     * @brief Skip to the next track
     * @return true if successful, false otherwise
     */
    bool next();

    /**
     * @brief Skip to the previous track
     * @return true if successful, false otherwise
     */
    bool previous();

    /**
     * @brief Set the playback volume
     * @param volume Volume level (0-100)
     * @return true if successful, false otherwise
     */
    bool setVolume(int volume);

    /**
     * @brief Get the name of the currently playing track
     * @return String containing the current track name
     */
    std::string getCurrentTrack();

    /**
     * @brief Get the current playback progress in milliseconds
     * @return Current progress in milliseconds
     */
    int getTrackProgress_ms();

    /**
     * @brief Toggle shuffle mode on/off
     * @return true if successful, false otherwise
     */
    bool toggleShuffle();

    /**
     * @brief Set the repeat mode
     * @param mode Repeat mode ("off", "context", "track")
     * @return true if successful, false otherwise
     */
    bool setRepeatMode(const std::string& mode);

    /**
     * @brief Add a track to the playback queue
     * @param track Track information to add to queue
     * @return true if successful, false otherwise
     */
    bool addToQueue(const TrackInfo& track);

    /**
     * @brief Request recently played tracks
     * @param index Starting index for the request
     * @param limit Maximum number of tracks to retrieve
     * @return true if successful, false otherwise
     */
    bool reqeustRecentlyPlayed(size_t index, size_t limit);

    /**
     * @brief Request the current playback queue
     * @param index Starting index for the request
     * @param limit Maximum number of tracks to retrieve
     * @return true if successful, false otherwise
     */
    bool requestQueue(size_t index, size_t limit);

    /**
     * @brief Update the currently playing track information
     * @return true if successful, false otherwise
     */
    bool updateCurrentlyPlaying();

    /**
     * @brief Update the playback state information
     * @return true if successful, false otherwise
     */
    bool updatePlaybackState();

    /**
     * @brief Refresh the Spotify API access token
     * @return true if successful, false otherwise
     */
    bool refreshToken()
    {
        return m_spotifyClient.refreshToken();
    }

    /**
     * @brief Print current status information to console
     */
    void printStatusString();

    /**
     * @brief Set the logging level for Spotify operations
     * @param level ESP-IDF log level to set
     */
    void setLogLevel(esp_log_level_t level);

    PlaybackState& getPlaybackState()
    {
        return m_playbackState;
    }
    CurrentlyPlayingInfo& getCurrentlyPlayingInfo()
    {
        return m_currentlyPlayingInfo;
    }

    void start_task();

private:
    const char*    TAG = "Spotify"; ///< Log tag for ESP-IDF logging
    SpotifyClient& m_spotifyClient =
        SpotifyClient::getInstance();              ///< Reference to the underlying Spotify client
    PlaybackState          m_playbackState;        ///< Current playback state information
    CurrentlyPlayingInfo   m_currentlyPlayingInfo; ///< Currently playing track information
    std::vector<TrackInfo> m_queue;                ///< Current playback queue
    std::vector<TrackInfo> m_recentlyPlayed;       ///< Recently played tracks

    /**
     * @brief Get current system timestamp in milliseconds
     * @return Current timestamp in milliseconds since system start
     */
    static uint64_t getCurrentTimestampMs()
    {
        return xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    friend int spotify_cmd(int    argc,
                           char** argv); ///< Allow console command access to private members

    void         task();
    TaskHandle_t m_task = NULL;
};

#endif // __SPOTIFY_HPP__
