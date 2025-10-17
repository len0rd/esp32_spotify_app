#ifndef __SPOTIFY_HPP__
#define __SPOTIFY_HPP__

#include <SpotifyClient.hpp>
#include <vector>

using json = nlohmann::json;

class Spotify
{
private:
    Spotify();
    ~Spotify();

public:
    // structs
    struct TrackInfo
    {
        std::string name;
        std::string artist;
        std::string album;
        std::string track_uri;
        int         duration_ms;
        std::string toString();
    };
    struct CurrentlyPlayingInfo
    {
        TrackInfo   currentTrack;
        std::string context_uri;
        int         progress_ms;
        uint64_t    timestamp_ms;
        int         getProgress_ms();
        int         getProgress_percent();
        std::string toString();
    };
    struct PlaybackState
    {
        bool        is_playing;
        bool        shuffle_state;
        std::string repeat_state;
        int         volume_percent;
        bool        supports_volume;
        std::string device_name;
        std::string device_id;
        std::string toString();
    };

    static Spotify& getInstance()
    {
        static Spotify instance;
        return instance;
    }

    bool        isPlaying();
    bool        play();
    bool        pause();
    bool        next();
    bool        previous();
    bool        setVolume(int volume);
    std::string getCurrentTrack();
    int         getTrackProgress_ms();
    bool        toggleShuffle();
    bool        setRepeatMode(const std::string& mode);
    bool        addToQueue(const TrackInfo& track);
    bool        reqeustRecentlyPlayed(size_t index, size_t limit);
    bool        requestQueue(size_t index, size_t limit);
    bool        updateCurrentlyPlaying();
    bool        getPlaybackState();
    bool        refreshToken()
    {
        return m_spotifyClient.refreshToken();
    }

    void printStatusString();
    void setLogLevel(esp_log_level_t level);

private:
    const char*            TAG             = "Spotify";
    SpotifyClient&         m_spotifyClient = SpotifyClient::getInstance();
    PlaybackState          m_playbackState;
    CurrentlyPlayingInfo   m_currentlyPlayingInfo;
    std::vector<TrackInfo> m_queue;
    std::vector<TrackInfo> m_recentlyPlayed;

    static uint64_t getCurrentTimestampMs()
    {
        return xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    friend int spotify_cmd(int argc, char** argv);
};

#endif // __SPOTIFY_HPP__
