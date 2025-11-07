/**
 * @file SpotifyPlaylist.hpp
 * @brief Spotify Playlist class
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyPlaylist class. This provides a high-level interface
 * for representing and managing individual Spotify playlists.
 */

#ifndef __SPOTIFYPLAYLIST_HPP__
#define __SPOTIFYPLAYLIST_HPP__

#include <vector>
#include <string>
#include "ui.h"
#include "lvgl.h"

class SpotifyPlaylist
{
public:
    static void playlist_clicked_cb(lv_event_t* e);
    SpotifyPlaylist();
    SpotifyPlaylist(const std::string& name, const std::string& uri);
    ~SpotifyPlaylist();
    void setPlaylistName(const std::string& name);

    // private:
    std::string playlist_name;
    std::string playlist_uri;
    lv_obj_t*   playlist_pannel;
    lv_obj_t*   playlist_label;
};

#endif // __SPOTIFYPLAYLIST_HPP__