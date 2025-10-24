#include <Spotify.hpp>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <wifi.h>

void spotify_cmd_init();

Spotify::Spotify()
{
    ESP_LOGD(TAG, "Spotify initialized");
    setLogLevel(ESP_LOG_INFO);
    spotify_cmd_init();
}
Spotify::~Spotify() {}
void Spotify::start_task()
{
    m_actionQueue = xQueueCreate(10, sizeof(SpotifyAction)); // 10 pending actions
    if (m_actionQueue == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create Spotify action queue");
        return;
    }
    xTaskCreate([](void* obj) { static_cast<Spotify*>(obj)->task(); }, "spotifyTask", 10 * 1024,
                this, 5, &m_task);
}
void Spotify::task()
{
    SpotifyAction action;

    while (true)
    {
        if (xQueueReceive(m_actionQueue, &action, pdMS_TO_TICKS(2000)) == pdPASS)
        {
            ESP_LOGD(TAG, "Processing Spotify action: %s", action.toString().c_str());
            switch (action.type)
            {
                case SpotifyActionType::Play:
                {
                    std::string body = "{\"context_uri\":\"" + m_currentlyPlayingInfo.context_uri +
                                       "\", \"offset\":{\"uri\":\"" +
                                       m_currentlyPlayingInfo.currentTrack.track_uri +
                                       "\"}, \"position_ms\":" +
                                       std::to_string(m_currentlyPlayingInfo.progress_ms) + "}";
                    bool ret =
                        m_spotifyClient.put("me/player/play", body, true).contains("error") ==
                        false;
                    if (ret)
                    {
                        m_playbackState.is_playing = true;
                    }
                    break;
                }

                case SpotifyActionType::Pause:
                {
                    bool ret =
                        m_spotifyClient.put("me/player/pause", "", true).contains("error") == false;
                    if (ret)
                    {
                        m_playbackState.is_playing = false;
                    }
                    break;
                }

                case SpotifyActionType::Next:
                {
                    m_spotifyClient.post("me/player/next", "", false);
                    updateCurrentlyPlaying();
                    break;
                }

                case SpotifyActionType::Previous:
                {
                    m_spotifyClient.post("me/player/previous", "", false);
                    updateCurrentlyPlaying();
                    break;
                }

                case SpotifyActionType::Seek:
                {
                    bool ret =
                        m_spotifyClient
                            .put("me/player/seek?position_ms=" + std::to_string(action.int_param),
                                 "", true)
                            .contains("error") == false;
                    if (ret)
                    {
                        m_currentlyPlayingInfo.progress_ms = action.int_param;
                    }
                    break;
                }

                case SpotifyActionType::SetVolume:
                {
                    if (!m_playbackState.supports_volume)
                    {
                        ESP_LOGW(TAG, "%s does not support volume adjustment",
                                 m_playbackState.device_name.c_str());
                        break;
                    }

                    bool ret = m_spotifyClient
                                   .put("me/player/volume?volume_percent=" +
                                            std::to_string(action.int_param),
                                        "", false)
                                   .contains("error") == false;
                    getPlaybackState();
                    break;
                }

                case SpotifyActionType::ToggleShuffle:
                {
                    m_spotifyClient.put(
                        "me/player/shuffle?state=" +
                            std::string(m_playbackState.shuffle_state ? "false" : "true"),
                        "", true);
                    updatePlaybackState();
                    break;
                }

                case SpotifyActionType::SetRepeatMode:
                {
                    std::string mode = action.str_param;
                    if (mode != "off" && mode != "context" && mode != "track")
                    {
                        ESP_LOGE(TAG, "Invalid repeat mode: %s", mode.c_str());
                        break;
                    }
                    m_spotifyClient.put("me/player/repeat?state=" + mode, "", false);
                    updatePlaybackState();
                    break;
                }

                case SpotifyActionType::UpdateCurrentlyPlaying:
                {
                    json response = m_spotifyClient.get("me/player/currently-playing");

                    if (response.contains("error"))
                    {
                        ESP_LOGE(TAG, "Error getting currently playing: %s",
                                 response["error"]["message"].get<std::string>().c_str());
                        break;
                    }
                    else if (response.is_null() || response.empty())
                    {
                        ESP_LOGI(TAG, "No track is currently playing");
                        break;
                    }

                    if (response.contains("item"))
                    {
                        if (response["item"].contains("name"))
                        {
                            m_currentlyPlayingInfo.currentTrack.name =
                                std::string(response["item"]["name"]);
                        }
                        if (response["item"].contains("artists") &&
                            response["item"]["artists"].is_array() &&
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
                            m_currentlyPlayingInfo.currentTrack.duration_ms =
                                response["item"]["duration_ms"];
                        }
                        if (response["item"].contains("uri"))
                        {
                            m_currentlyPlayingInfo.currentTrack.track_uri =
                                std::string(response["item"]["uri"]);
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
                            m_currentlyPlayingInfo.context_uri =
                                std::string(response["context"]["uri"]);
                        }
                    }

                    break;
                }

                case SpotifyActionType::UpdatePlaybackState:
                {
                    json response = m_spotifyClient.get("me/player");

                    if (response.contains("error"))
                    {
                        ESP_LOGE(TAG, "Error getting playback state: %s",
                                 response["error"]["message"].get<std::string>().c_str());
                        break;
                    }
                    else if (response.is_null() || response.empty())
                    {
                        ESP_LOGI(TAG, "No playback state available");
                        break;
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
                        // m_playbackState.volume_percent
                        if (response["device"].contains("volume_percent"))
                        {
                            m_playbackState.volume_percent = response["device"]["volume_percent"];
                        }
                        // m_playbackState.supports_volume
                        if (response["device"].contains("supports_volume"))
                        {
                            m_playbackState.supports_volume = response["device"]["supports_volume"];
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
        else
        {
            // update periodically if no other actions
            updateCurrentlyPlaying();
            updatePlaybackState();
        }
    }
}
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
int Spotify::CurrentlyPlayingInfo::getRemaining_ms()
{
    return currentTrack.duration_ms - getProgress_ms();
}
float Spotify::CurrentlyPlayingInfo::getProgress_percent()
{
    if (currentTrack.duration_ms == 0)
    {
        return 0;
    }
    return (float) (getProgress_ms()) / (float) currentTrack.duration_ms;
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
    SpotifyAction action = {SpotifyActionType::Pause};
    bool          ret    = xQueueSend(m_actionQueue, &action, 0) == pdPASS;
    updatePlaybackState();
    return ret;
}
bool Spotify::next()
{
    SpotifyAction action = {SpotifyActionType::Next};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::previous()
{
    SpotifyAction action = {SpotifyActionType::Previous};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::setVolume(int volume)
{
    SpotifyAction action = {SpotifyActionType::SetVolume, "", volume};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::updateCurrentlyPlaying()
{
    SpotifyAction action = {SpotifyActionType::UpdateCurrentlyPlaying};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::play()
{
    SpotifyAction action = {SpotifyActionType::Play};
    bool          ret    = xQueueSend(m_actionQueue, &action, 0) == pdPASS;
    updateCurrentlyPlaying();
    updatePlaybackState();
    return ret;
}
bool Spotify::updatePlaybackState()
{
    SpotifyAction action = {SpotifyActionType::UpdatePlaybackState};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::toggleShuffle()
{
    SpotifyAction action = {SpotifyActionType::ToggleShuffle};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::setRepeatMode(const std::string& mode)
{
    SpotifyAction action = {SpotifyActionType::SetRepeatMode, mode};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::seek(int position_ms)
{
    SpotifyAction action = {SpotifyActionType::Seek, "", position_ms};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
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
           std::to_string(100 * getProgress_percent()) + "%)";
}
std::string Spotify::PlaybackState::toString()
{
    return "Playback State - Playing: " + std::to_string(is_playing) +
           ", Volume: " + std::to_string(volume_percent) + "%" +
           ", Supports_Volume: " + std::to_string(supports_volume) +
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
    m_spotifyClient.setLogLevel(level);
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
    .command = "spotify",
    .help = "spotify command: 'status', 'play', 'pause', 'next', 'previous', 'shuffle', 'update', "
            "'refreshToken', 'repeat'",
    .hint = NULL,
    .func = &spotify_cmd,
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
    int nerrors = arg_parse(argc, argv, (void**) &s_spotify_cmd_args);
    if (nerrors != 0)
    {
        // arg_print_errors(stderr, s_spotify_cmd_args->end, argv[0]);
        printf("Usage: spotify <action>\n");
        return 1;
    }

    const char* action = s_spotify_cmd_args.action->sval[0];

    if (strcmp(action, "status") == 0)
    {
        Spotify::getInstance().printStatusString();
    }
    else if (strcmp(action, "play") == 0)
    {
        Spotify::getInstance().play();
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
    else if (strcmp(action, "shuffle") == 0)
    {
        Spotify::getInstance().toggleShuffle();
    }
    else if (strcmp(action, "update") == 0)
    {
        Spotify::getInstance().updateCurrentlyPlaying();
        Spotify::getInstance().updatePlaybackState();
    }
    else if (strcmp(action, "refreshToken") == 0)
    {
        Spotify::getInstance().refreshToken();
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
