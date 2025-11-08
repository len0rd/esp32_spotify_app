/**
 * @file SpotifyPlaylistItem.hpp
 * @brief Spotify Queue Item class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyPlaylistItem class which provides a high-level interface
 * for representing and managing individual items in a Spotify playback queue. It handles
 * the display and interaction logic for queue items within the user interface.
 */

#ifndef __SPOTIFYPLAYLISTITEM_HPP__
#define __SPOTIFYPLAYLISTITEM_HPP__

#include <vector>
#include <string>
#include "ui.h"
#include "lvgl.h"

class SpotifyPlaylistItem
{
public:
    static void song_clicked_cb(lv_event_t* e);
    static void song_queue_add_clicked_cb(lv_event_t* e);
    SpotifyPlaylistItem();
    SpotifyPlaylistItem(const std::string& song, const std::string& artist, const std::string& uri,
                        const std::string& playlist_uri);
    ~SpotifyPlaylistItem();
    void setSongName(const std::string& name);
    void setArtistName(const std::string& name);

    // private:
    std::string song_name;
    std::string artist_name;
    std::string song_uri;
    std::string parent_playlist_uri;
    lv_obj_t*   playlist_item_pannel;
    lv_obj_t*   labels_container;
    lv_obj_t*   song_label;
    lv_obj_t*   artist_label;
    lv_obj_t*   add_to_queue_btn;
};

#endif // __SPOTIFYPLAYLISTITEM_HPP__