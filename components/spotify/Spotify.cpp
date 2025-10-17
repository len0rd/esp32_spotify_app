#include <Spotify.hpp>
#include "esp_console.h"
#include "argtable3/argtable3.h"

void spotify_cmd_init();

Spotify::Spotify()
{
    setLogLevel(ESP_LOG_DEBUG);
    spotify_cmd_init();
}
Spotify::~Spotify() {}
int Spotify::CurrentlyPlayingInfo::getProgress_ms()
{
    if (!Spotify::getInstance().isPlaying())
    {
        return progress_ms;
    }
    uint64_t current_time_ms     = getCurrentTimestampMs();
    int      elapsed_ms          = static_cast<int>(current_time_ms - timestamp_ms);
    int      current_progress_ms = progress_ms + elapsed_ms;
    if (current_progress_ms > currentTrack.duration_ms)
    {
        current_progress_ms = currentTrack.duration_ms;
    }
    return current_progress_ms;
}
int Spotify::CurrentlyPlayingInfo::getProgress_percent()
{
    if (currentTrack.duration_ms == 0)
    {
        return 0;
    }
    return (getProgress_ms() * 100) / currentTrack.duration_ms;
}
bool Spotify::isPlaying()
{
    return m_playbackState.is_playing;
}
std::string Spotify::getCurrentTrack()
{
    return m_currentlyPlayingInfo.currentTrack.name;
}
int Spotify::getTrackProgress_ms()
{
    return m_currentlyPlayingInfo.getProgress_ms();
}
bool Spotify::pause()
{
    updateCurrentlyPlaying();
    bool ret = m_spotifyClient.put("me/player/pause", "", true).contains("error") == false;
    if (ret)
    {
        m_playbackState.is_playing = false;
    }
    getPlaybackState();
    return ret;
}
bool Spotify::next()
{
    return m_spotifyClient.post("me/player/next").contains("error") == false;
}
bool Spotify::previous()
{
    return m_spotifyClient.post("me/player/previous").contains("error") == false;
}
bool Spotify::setVolume(int volume)
{
    json body = {{"volume_percent", volume}};
    return m_spotifyClient.put("me/player/volume", body).contains("error") == false;
}
bool Spotify::updateCurrentlyPlaying()
{
    json response = m_spotifyClient.get("me/player/currently-playing");

    if (response.contains("error"))
    {
        ESP_LOGE(TAG, "Error getting currently playing: %s",
                 response["error"]["message"].get<std::string>().c_str());
        return false;
    }
    else if (response.is_null() || response.empty())
    {
        ESP_LOGI(TAG, "No track is currently playing");
        return false;
    }

    if (response.contains("item"))
    {
        if (response["item"].contains("name"))
        {
            m_currentlyPlayingInfo.currentTrack.name = std::string(response["item"]["name"]);
        }
        if (response["item"].contains("artists") && response["item"]["artists"].is_array() &&
            !response["item"]["artists"].empty())
        {
            if (response["item"]["artists"][0].contains("name"))
            {
                m_currentlyPlayingInfo.currentTrack.artist =
                    std::string(response["item"]["artists"][0]["name"]);
            }
        }
        if (response["item"].contains("album"))
        {
            if (response["item"]["album"].contains("name"))
            {
                m_currentlyPlayingInfo.currentTrack.album =
                    std::string(response["item"]["album"]["name"]);
            }
        }
        if (response["item"].contains("duration_ms"))
        {
            m_currentlyPlayingInfo.currentTrack.duration_ms = response["item"]["duration_ms"];
        }
        if (response["item"].contains("uri"))
        {
            m_currentlyPlayingInfo.currentTrack.track_uri = std::string(response["item"]["uri"]);
        }

        if (response.contains("progress_ms"))
        {
            m_currentlyPlayingInfo.progress_ms  = response["progress_ms"];
            m_currentlyPlayingInfo.timestamp_ms = getCurrentTimestampMs();
        }
    }

    if (response.contains("context"))
    {
        if (response["context"].contains("uri"))
        {
            m_currentlyPlayingInfo.context_uri = std::string(response["context"]["uri"]);
        }
    }

    return true;
}
bool Spotify::play()
{
    std::string body =
        "{\"context_uri\":\"" + m_currentlyPlayingInfo.context_uri + "\", \"offset\":{\"uri\":\"" +
        m_currentlyPlayingInfo.currentTrack.track_uri +
        "\"}, \"position_ms\":" + std::to_string(m_currentlyPlayingInfo.progress_ms) + "}";
    bool ret = m_spotifyClient.put("me/player/play", body, true).contains("error") == false;
    if (ret)
    {
        m_playbackState.is_playing = true;
    }
    updateCurrentlyPlaying();
    getPlaybackState();
    return ret;
}
bool Spotify::getPlaybackState()
{
    json response = m_spotifyClient.get("me/player");

    if (response.contains("error"))
    {
        ESP_LOGE(TAG, "Error getting playback state: %s",
                 response["error"]["message"].get<std::string>().c_str());
        return false;
    }
    else if (response.is_null() || response.empty())
    {
        ESP_LOGI(TAG, "No playback state available");
        return false;
    }

    // m_playbackState.is_playing
    if (response.contains("is_playing"))
    {
        m_playbackState.is_playing = response["is_playing"];
    }
    // m_playbackState.shuffle_state
    if (response.contains("shuffle_state"))
    {
        m_playbackState.shuffle_state = response["shuffle_state"];
    }
    // m_playbackState.repeat_state
    if (response.contains("repeat_state"))
    {
        m_playbackState.repeat_state = response["repeat_state"];
    }
    // m_playbackState.volume_percent
    if (response.contains("volume_percent"))
    {
        m_playbackState.volume_percent = response["volume_percent"];
    }
    // m_playbackState.supports_volume
    if (response.contains("supports_volume"))
    {
        m_playbackState.supports_volume = response["supports_volume"];
    }
    if (response.contains("device"))
    {
        // m_playbackState.device_name
        if (response["device"].contains("name"))
        {
            m_playbackState.device_name = response["device"]["name"];
        }
        // m_playbackState.device_id
        if (response["device"].contains("id"))
        {
            m_playbackState.device_id = response["device"]["id"];
        }
    }
    return true;
}
bool Spotify::toggleShuffle()
{
    if (m_spotifyClient
            .put("me/player/shuffle?state=" +
                 std::string(m_playbackState.shuffle_state ? "false" : "true"))
            .contains("error"))
        return false;
    return getPlaybackState();
}
bool Spotify::setRepeatMode(const std::string& mode)
{
    if (mode != "off" && mode != "context" && mode != "track")
    {
        ESP_LOGE(TAG, "Invalid repeat mode: %s", mode.c_str());
        return false;
    }
    if (m_spotifyClient.put("me/player/rpeat?state=" + mode).contains("error"))
        return false;
    return getPlaybackState();
}
bool Spotify::addToQueue(const TrackInfo&)
{
    return false;
}
bool Spotify::reqeustRecentlyPlayed(size_t index, size_t limit)
{
    return false;
}
bool Spotify::requestQueue(size_t index, size_t limit)
{
    return false;
}
std::string Spotify::TrackInfo::toString()
{
    return "Track: " + name + ", Artist: " + artist + ", Album: " + album;
}
std::string Spotify::CurrentlyPlayingInfo::toString()
{
    return "Currently Playing: " + currentTrack.toString() + ", Context: " + context_uri +
           ", Progress: " + std::to_string(getProgress_ms()) + " ms (" +
           std::to_string(getProgress_percent()) + "%)";
}
std::string Spotify::PlaybackState::toString()
{
    return "Playback State - Playing: " + std::to_string(is_playing) +
           ", Volume: " + std::to_string(volume_percent) + "%" +
           ", Shuffle: " + std::to_string(shuffle_state) + ", Repeat: " + repeat_state +
           ", Device: " + device_name;
}
void Spotify::printStatusString()
{
    printf("%s\n", m_playbackState.toString().c_str());
    printf("%s\n", m_currentlyPlayingInfo.toString().c_str());
}
void Spotify::setLogLevel(esp_log_level_t level)
{
    esp_log_level_set(TAG, level);
}

/****************************************************************************/
/************************ SPOTIFY CONSOLE COMMAND ***************************/
/****************************************************************************/
int spotify_cmd(int argc, char** argv);

static struct
{
    struct arg_str* action;
    // struct arg_str* inputStr;
    struct arg_end* end;
} s_spotify_cmd_args;
static esp_console_cmd_t s_spotify_cmd_struct{
    .command  = "spotify",
    .help     = "spotify command: 'play', 'pause', 'next', 'previous', 'status', 'shuffle', "
                "'repeat'",
    .hint     = NULL,
    .func     = &spotify_cmd,
    .argtable = &s_spotify_cmd_args,
    .context  = NULL,
};

void spotify_cmd_init()
{
    // s_spotify_cmd_args = (decltype(s_spotify_cmd_args)) calloc(1, sizeof(*s_spotify_cmd_args));
    s_spotify_cmd_args.action = arg_str1(NULL, NULL, "<action>",
                                         "Spotify action: 'play', 'pause', 'next', 'previous', "
                                         "'status', 'shuffle', 'repeat'");
    s_spotify_cmd_args.end    = arg_end(2);

    ESP_ERROR_CHECK(esp_console_cmd_register(&s_spotify_cmd_struct));
}
int spotify_cmd(int argc, char** argv)
{
    // isPlaying();
    // getCurrentTrack();
    // getTrackProgress_ms();

    // play();
    // pause();
    // next();
    // previous();
    // setVolume();
    // toggleShuffle();
    // setRepeatMode();
    // reqeustRecentlyPlayed();
    // updateCurrentlyPlaying();
    // getPlaybackState();
    // addToQueue();
    // requestQueue();
    int nerrors = arg_parse(argc, argv, (void**) &s_spotify_cmd_args);
    if (nerrors != 0)
    {
        // arg_print_errors(stderr, s_spotify_cmd_args->end, argv[0]);
        printf("Usage: spotify <action>\n");
        return 1;
    }

    const char* action = s_spotify_cmd_args.action->sval[0];

    if (strcmp(action, "play") == 0)
    {
        Spotify::getInstance().play();
    }
    else if (strcmp(action, "refreshToken") == 0)
    {
        Spotify::getInstance().refreshToken();
    }
    else if (strcmp(action, "getPlaying") == 0)
    {
        Spotify::getInstance().updateCurrentlyPlaying();
    }
    else if (strcmp(action, "pause") == 0)
    {
        Spotify::getInstance().pause();
    }
    else if (strcmp(action, "next") == 0)
    {
        Spotify::getInstance().next();
    }
    else if (strcmp(action, "previous") == 0)
    {
        Spotify::getInstance().previous();
    }
    else if (strcmp(action, "status") == 0)
    {
        Spotify::getInstance().printStatusString();
    }
    else if (strcmp(action, "shuffle") == 0)
    {
        Spotify::getInstance().toggleShuffle();
    }
    else if (strcmp(action, "repeat") == 0)
    {
        // toggle between off, context, track
        std::string new_mode;
        if (Spotify::getInstance().m_playbackState.repeat_state == "off")
        {
            new_mode = "context";
        }
        else if (Spotify::getInstance().m_playbackState.repeat_state == "context")
        {
            new_mode = "track";
        }
        else
        {
            new_mode = "off";
        }
        Spotify::getInstance().setRepeatMode(new_mode);
    }
    else
    {
        printf("Unknown action: %s\n", action);
        return 1;
    }

    return 0;
}
