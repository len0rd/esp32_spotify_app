#include <Spotify.hpp>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <wifi.h>

void spotify_cmd_init();

Spotify::Spotify()
{
    setLogLevel(ESP_LOG_WARN);
    spotify_cmd_init();
}
Spotify::~Spotify() {}
void Spotify::start_task()
{
    m_actionQueue = xQueueCreate(10, sizeof(SpotifyAction*)); // 10 pending actions
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
    SpotifyAction* action;
    while (true)
    {
        if (xQueueReceive(m_actionQueue, &action, pdMS_TO_TICKS(2000)) == pdPASS)
        {
            esp_log_level_t previous_level = esp_log_level_get(TAG);
            if (action->verbose)
            {
                setLogLevel(ESP_LOG_INFO);
            }
            ESP_LOGI(TAG, "Processing Spotify action: %s", action->toString().c_str());
            switch (action->type)
            {
                case SpotifyActionType::Play:
                {
                    json body = {{"context_uri", action->context_uri.empty()
                                                     ? m_currentlyPlayingInfo.context_uri
                                                     : action->context_uri},
                                 {"offset",
                                  {{"uri", action->song_uri.empty()
                                               ? m_currentlyPlayingInfo.currentTrack.track_uri
                                               : action->song_uri}}},
                                 {"position_ms", action->song_uri.empty()
                                                     ? m_currentlyPlayingInfo.progress_ms
                                                     : 0}};
                    auto resp = m_spotifyClient.put("me/player/play", body.dump(), true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_playbackState.is_playing = true;
                    }
                }
                break;

                case SpotifyActionType::Pause:
                {
                    auto resp = m_spotifyClient.put("me/player/pause", "", true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_playbackState.is_playing = false;
                    }
                }
                break;

                case SpotifyActionType::Next:
                {
                    m_spotifyClient.post("me/player/next", "", true);
                    updateCurrentlyPlaying();
                }
                break;

                case SpotifyActionType::Previous:
                {
                    m_spotifyClient.post("me/player/previous", "", true);
                    updateCurrentlyPlaying();
                }
                break;

                case SpotifyActionType::Seek:
                {
                    auto resp = m_spotifyClient.put("me/player/seek?position_ms=" +
                                                        std::to_string(action->int_param),
                                                    "", true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_currentlyPlayingInfo.progress_ms = action->int_param;
                    }
                }
                break;

                case SpotifyActionType::SetVolume:
                {
                    if (!m_playbackState.supports_volume)
                    {
                        ESP_LOGW(TAG, "%s does not support volume adjustment",
                                 m_playbackState.device_name.c_str());
                        break;
                    }

                    m_spotifyClient.put("me/player/volume?volume_percent=" +
                                            std::to_string(action->int_param),
                                        "", false);
                    getPlaybackState();
                }
                break;

                case SpotifyActionType::ToggleShuffle:
                {
                    m_spotifyClient.put(
                        "me/player/shuffle?state=" +
                            std::string(m_playbackState.shuffle_state ? "false" : "true"),
                        "", true);
                    updatePlaybackState();
                }
                break;

                case SpotifyActionType::SetRepeatMode:
                {
                    std::string mode = action->str_param;
                    if (mode != "off" && mode != "context" && mode != "track")
                    {
                        ESP_LOGE(TAG, "Invalid repeat mode: %s", mode.c_str());
                        break;
                    }
                    m_spotifyClient.put("me/player/repeat?state=" + mode, "", false);
                    updatePlaybackState();
                }
                break;

                case SpotifyActionType::UpdateCurrentlyPlaying:
                {
                    auto resp = m_spotifyClient.get("me/player/currently-playing");
                    if (resp)
                    {
                        if (resp->contains("error"))
                        {
                            ESP_LOGE(TAG, "Error getting currently playing: %s",
                                     (*resp)["error"]["message"].get<std::string>().c_str());
                            break;
                        }
                        else if (resp->is_null() || resp->empty())
                        {
                            ESP_LOGI(TAG, "No track is currently playing");
                            break;
                        }

                        if (resp->contains("item"))
                        {
                            if ((*resp)["item"].contains("name"))
                            {
                                m_currentlyPlayingInfo.currentTrack.name =
                                    std::string((*resp)["item"]["name"]);
                            }
                            if ((*resp)["item"].contains("artists") &&
                                (*resp)["item"]["artists"].is_array() &&
                                !(*resp)["item"]["artists"].empty())
                            {
                                if ((*resp)["item"]["artists"][0].contains("name"))
                                {
                                    m_currentlyPlayingInfo.currentTrack.artist =
                                        std::string((*resp)["item"]["artists"][0]["name"]);
                                }
                            }
                            if ((*resp)["item"].contains("album"))
                            {
                                if ((*resp)["item"]["album"].contains("name"))
                                {
                                    m_currentlyPlayingInfo.currentTrack.album =
                                        std::string((*resp)["item"]["album"]["name"]);
                                }
                            }
                            if ((*resp)["item"].contains("duration_ms"))
                            {
                                m_currentlyPlayingInfo.currentTrack.duration_ms =
                                    (*resp)["item"]["duration_ms"];
                            }
                            if ((*resp)["item"].contains("uri"))
                            {
                                m_currentlyPlayingInfo.currentTrack.track_uri =
                                    std::string((*resp)["item"]["uri"]);
                            }

                            if (resp->contains("progress_ms"))
                            {
                                m_currentlyPlayingInfo.progress_ms  = (*resp)["progress_ms"];
                                m_currentlyPlayingInfo.timestamp_ms = getCurrentTimestampMs();
                            }
                        }

                        if (resp->contains("context"))
                        {
                            if ((*resp)["context"].contains("uri"))
                            {
                                m_currentlyPlayingInfo.context_uri =
                                    std::string((*resp)["context"]["uri"]);
                            }
                        }
                        ESP_LOGI(TAG, "Updated currently playing: %s",
                                 m_currentlyPlayingInfo.toString().c_str());
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get currently playing track");
                    }
                }
                break;

                case SpotifyActionType::UpdatePlaybackState:
                {
                    auto resp = m_spotifyClient.get("me/player");
                    if (resp)
                    {
                        if (resp->contains("error"))
                        {
                            ESP_LOGE(TAG, "Error getting playback state: %s",
                                     (*resp)["error"]["message"].get<std::string>().c_str());
                            break;
                        }
                        else if (resp->is_null() || resp->empty())
                        {
                            ESP_LOGI(TAG, "No playback state available");
                            break;
                        }

                        // m_playbackState.is_playing
                        if (resp->contains("is_playing"))
                        {
                            m_playbackState.is_playing = (*resp)["is_playing"];
                        }
                        // m_playbackState.shuffle_state
                        if (resp->contains("shuffle_state"))
                        {
                            m_playbackState.shuffle_state = (*resp)["shuffle_state"];
                        }
                        // m_playbackState.repeat_state
                        if (resp->contains("repeat_state"))
                        {
                            m_playbackState.repeat_state = (*resp)["repeat_state"];
                        }
                        if (resp->contains("device"))
                        {
                            // m_playbackState.device_name
                            if ((*resp)["device"].contains("name"))
                            {
                                m_playbackState.device_name = (*resp)["device"]["name"];
                            }
                            // m_playbackState.device_id
                            if ((*resp)["device"].contains("id"))
                            {
                                m_playbackState.device_id = (*resp)["device"]["id"];
                            }
                            // m_playbackState.volume_percent
                            if ((*resp)["device"].contains("volume_percent"))
                            {
                                m_playbackState.volume_percent =
                                    (*resp)["device"]["volume_percent"];
                            }
                            // m_playbackState.supports_volume
                            if ((*resp)["device"].contains("supports_volume"))
                            {
                                m_playbackState.supports_volume =
                                    (*resp)["device"]["supports_volume"];
                            }
                        }
                        ESP_LOGI(TAG, "Updated playback state: %s",
                                 m_playbackState.toString().c_str());
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get currently playing track");
                    }
                }
                break;

                case SpotifyActionType::GetQueue:
                {
                    auto resp = m_spotifyClient.get("me/player/queue");

                    if (resp && resp->contains("error") == false)
                    {
                        if (resp->contains("queue") && (*resp)["queue"].is_array())
                        {
                            m_songQueue.clear();
                            for (auto& item : (*resp)["queue"])
                            {
                                if (item.contains("name") && item.contains("artists") &&
                                    item["artists"].is_array() && !item["artists"].empty() &&
                                    item.contains("uri"))
                                {
                                    if (item.contains("album") &&
                                        item["album"].contains("available_markets"))
                                        item["album"].erase("available_markets");
                                    if (item.contains("available_markets"))
                                        item.erase("available_markets");

                                    std::string artist_name =
                                        item["artists"][0]["name"].get<std::string>();
                                    std::string song_name = item["name"].get<std::string>();
                                    std::string uri       = item["uri"].get<std::string>();
                                    m_songQueue.push_back(std::make_unique<SpotifyQueueItem>(
                                        song_name, artist_name, uri));
                                }
                            }
                        }
                    }
                }
                break;

                default:
                    break;
            }
            if (action->verbose)
            {
                setLogLevel(previous_level);
            }
            delete action;
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
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Pause, m_verbose};
    bool           ret    = xQueueSend(m_actionQueue, &action, 0) == pdPASS;
    updatePlaybackState();
    return ret;
}
bool Spotify::next()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Next, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::previous()
{
    // If the current track has been playing for more than 5 seconds, seek to the beginning
    if (getCurrentlyPlayingInfo().getProgress_ms() > 5000)
    {
        return seek(0);
    }
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Previous, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::setVolume(int volume)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::SetVolume, m_verbose};
    action->int_param     = volume;
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::updateCurrentlyPlaying()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::UpdateCurrentlyPlaying, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::play(std::string song_uri, std::string context_uri)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Play, m_verbose};
    action->song_uri      = song_uri;
    action->context_uri   = context_uri;
    bool ret              = xQueueSend(m_actionQueue, &action, 0) == pdPASS;
    updateCurrentlyPlaying();
    updatePlaybackState();
    return ret;
}
bool Spotify::updatePlaybackState()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::UpdatePlaybackState, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::toggleShuffle()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::ToggleShuffle, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::setRepeatMode(const std::string& mode)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::SetRepeatMode, m_verbose};
    action->str_param     = mode;
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
}
bool Spotify::seek(int position_ms)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Seek, m_verbose};
    action->int_param     = position_ms;
    if (xQueueSend(m_actionQueue, &action, 0) == pdPASS)
    {
        getCurrentlyPlayingInfo().progress_ms = position_ms;
        return updateCurrentlyPlaying();
    }
    return false;
}
bool Spotify::addToQueue(const TrackInfo&)
{
    return false;
}
bool Spotify::reqeustRecentlyPlayed(size_t index, size_t limit)
{
    return false;
}
bool Spotify::requestQueue()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetQueue, m_verbose};
    return xQueueSend(m_actionQueue, &action, 0) == pdPASS;
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

const char* SPOTIFY_USAGE_STRING = "Spotify command usage:\n"
                                   "  spotify <action>\n"
                                   "    action: 'play', 'pause', 'next', 'previous', 'status', "
                                   "'shuffle', 'update', 'getQueue', 'refreshToken', "
                                   "'repeat'\n";
static struct
{
    struct arg_str* action;
    // struct arg_str* inputStr;
    struct arg_end* end;
} s_spotify_cmd_args;
static esp_console_cmd_t s_spotify_cmd_struct{
    .command  = "spotify",
    .help     = "Manually run Spotify actions from console",
    .hint     = NULL,
    .func     = &spotify_cmd,
    .argtable = &s_spotify_cmd_args,
    .context  = NULL,
};

void spotify_cmd_init()
{
    // s_spotify_cmd_args = (decltype(s_spotify_cmd_args)) calloc(1, sizeof(*s_spotify_cmd_args));
    s_spotify_cmd_args.action =
        arg_str1(NULL, NULL, "<action>",
                 "action: 'play', 'pause', 'next', 'previous', 'status', 'shuffle', 'update', "
                 "'getQueue', 'refreshToken', 'repeat'");
    s_spotify_cmd_args.end = arg_end(2);

    ESP_ERROR_CHECK(esp_console_cmd_register(&s_spotify_cmd_struct));
}
int spotify_cmd(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &s_spotify_cmd_args);
    if (nerrors != 0)
    {
        printf(SPOTIFY_USAGE_STRING);
        return 1;
    }

    const char* action = s_spotify_cmd_args.action->sval[0];

    Spotify& sp = Spotify::getInstance();

    bool wasVerbose = sp.isVerbose();
    sp.setVerbose(true);
    if (strcmp(action, "status") == 0)
    {
        sp.printStatusString();
    }
    else if (strcmp(action, "play") == 0)
    {
        sp.play();
    }
    else if (strcmp(action, "pause") == 0)
    {
        sp.pause();
    }
    else if (strcmp(action, "next") == 0)
    {
        sp.next();
    }
    else if (strcmp(action, "previous") == 0)
    {
        sp.previous();
    }
    else if (strcmp(action, "shuffle") == 0)
    {
        sp.toggleShuffle();
    }
    else if (strcmp(action, "getQueue") == 0)
    {
        sp.requestQueue();
    }
    else if (strcmp(action, "update") == 0)
    {
        sp.updateCurrentlyPlaying();
        sp.updatePlaybackState();
    }
    else if (strcmp(action, "refreshToken") == 0)
    {
        sp.refreshToken();
    }
    else if (strcmp(action, "repeat") == 0)
    {
        // toggle between off, context, track
        std::string new_mode;
        if (sp.m_playbackState.repeat_state == "off")
        {
            new_mode = "context";
        }
        else if (sp.m_playbackState.repeat_state == "context")
        {
            new_mode = "track";
        }
        else
        {
            new_mode = "off";
        }
        sp.setRepeatMode(new_mode);
    }
    else
    {
        printf("Unknown action: %s\n", action);
        printf(SPOTIFY_USAGE_STRING);
    }

    sp.setVerbose(wasVerbose);

    return 0;
}
