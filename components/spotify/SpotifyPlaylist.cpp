/**
 * @file SpotifyPlaylist.cpp
 * @brief Spotify Playlist class
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyPlaylist class. This provides a high-level interface
 * for representing and managing individual Spotify playlists.
 */

#include <SpotifyPlaylist.hpp>
#include "display_init.h"
#include <Spotify.hpp>

void SpotifyPlaylist::playlist_clicked_cb(lv_event_t* e)
{
    lv_event_code_t  event_code = lv_event_get_code(e);
    SpotifyPlaylist* playlist   = static_cast<SpotifyPlaylist*>(lv_event_get_user_data(e));

    if (event_code == LV_EVENT_CLICKED)
    {
        // return playlist screen to top
        lv_obj_scroll_to_y(ui_Songs_Container, 0, LV_ANIM_OFF);
        lv_obj_scroll_by(ui_Songs_Container, 0, -100, LV_ANIM_ON);

        Spotify::getInstance().requestPlaylist(playlist->playlist_uri);

        // clear old playlist items
        Spotify::getInstance().m_playlistItems.clear();

        lv_label_set_text(ui_Playlist_Title_Name_Label, playlist->playlist_name.c_str());
        _ui_screen_change(&ui_PlayList_Screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                          &ui_PlayList_Screen_screen_init);
    }
}
SpotifyPlaylist::SpotifyPlaylist() : SpotifyPlaylist{"", ""} {}
SpotifyPlaylist::SpotifyPlaylist(const std::string& name, const std::string& uri)
    : playlist_name{name}, playlist_uri{uri}
{
    ui_lvgl_lock(-1);

    playlist_pannel = lv_obj_create(ui_Playlists_Container);
    lv_obj_set_width(playlist_pannel, 360);
    lv_obj_set_height(playlist_pannel, 50);
    lv_obj_set_align(playlist_pannel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(playlist_pannel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(playlist_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(playlist_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(playlist_pannel, lv_color_hex(0x413C3C),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(playlist_pannel, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(playlist_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(playlist_pannel, 150, LV_PART_MAIN | LV_STATE_PRESSED);

    playlist_label = lv_label_create(playlist_pannel);
    lv_obj_set_width(playlist_label, LV_SIZE_CONTENT);
    lv_obj_set_height(playlist_label, LV_SIZE_CONTENT);
    lv_obj_set_align(playlist_label, LV_ALIGN_CENTER);
    lv_label_set_text(playlist_label, playlist_name.c_str());
    lv_obj_set_style_text_font(playlist_label, &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(playlist_pannel, playlist_clicked_cb, LV_EVENT_ALL, this);

    // unscroll by the same amount as adding the new item
    lv_obj_scroll_by(ui_Playlists_Container, 0, 50 / 2, LV_ANIM_OFF);

    ui_lvgl_unlock();
}
SpotifyPlaylist::~SpotifyPlaylist()
{
    ui_lvgl_lock(-1);
    lv_obj_remove_event_cb(playlist_pannel, playlist_clicked_cb);
    lv_obj_del(playlist_pannel);

    // unscroll by the same amount as deleting the new item
    lv_obj_scroll_by(ui_Playlists_Container, 0, -50 / 2, LV_ANIM_OFF);
    ui_lvgl_unlock();
}
void SpotifyPlaylist::setPlaylistName(const std::string& name)
{
    ui_lvgl_lock(-1);
    playlist_name = name;
    lv_label_set_text(playlist_label, playlist_name.c_str());
    ui_lvgl_unlock();
}
