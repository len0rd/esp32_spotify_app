/**
 * @file SpotifyPlaylistItem.cpp
 * @brief Spotify Queue Item class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyPlaylistItem class which provides a high-level interface
 * for representing and managing individual items in a Spotify playback queue. It handles
 * the display and interaction logic for queue items within the user interface.
 */

#include <SpotifyPlaylistItem.hpp>
#include <Spotify.hpp>
#include <display_init.h>

void SpotifyPlaylistItem::song_clicked_cb(lv_event_t* e)
{
    SpotifyPlaylistItem* data = static_cast<SpotifyPlaylistItem*>(lv_event_get_user_data(e));
    lv_event_code_t      code = lv_event_get_code(e);
    if (data && code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().play(data->song_uri);
        Spotify::getInstance().requestQueue();
    }
}
void SpotifyPlaylistItem::song_queue_add_clicked_cb(lv_event_t* e)
{
    SpotifyPlaylistItem* data = static_cast<SpotifyPlaylistItem*>(lv_event_get_user_data(e));
    lv_event_code_t      code = lv_event_get_code(e);
    if (data && code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().addToQueue(data->song_uri);
        Spotify::getInstance().requestQueue();
    }
}

SpotifyPlaylistItem::SpotifyPlaylistItem()
    : SpotifyPlaylistItem("Song Name", "Artist Name", "Song URI")
{
}
SpotifyPlaylistItem::SpotifyPlaylistItem(const std::string& song, const std::string& artist,
                                         const std::string& uri)
    : song_name(song), artist_name(artist), song_uri(uri)
{
    // assert(ui_sem && "bsp_display_start must be called first");
    // xSemaphoreTake(ui_sem, portMAX_DELAY);
    ui_lvgl_lock(-1);

    playlist_item_pannel = lv_obj_create(ui_Songs_Container);
    lv_obj_set_width(playlist_item_pannel, 360);
    lv_obj_set_height(playlist_item_pannel, 45);
    lv_obj_set_align(playlist_item_pannel, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(playlist_item_pannel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(playlist_item_pannel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(playlist_item_pannel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(playlist_item_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(playlist_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(playlist_item_pannel, lv_color_hex(0x413C3C),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(playlist_item_pannel, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(playlist_item_pannel, 45, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(playlist_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(playlist_item_pannel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(playlist_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(playlist_item_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(playlist_item_pannel, 150, LV_PART_MAIN | LV_STATE_PRESSED);

    labels_container = lv_obj_create(playlist_item_pannel);
    lv_obj_remove_style_all(labels_container);
    lv_obj_set_width(labels_container, 205);
    lv_obj_set_height(labels_container, 32);
    lv_obj_set_align(labels_container, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(labels_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(labels_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(labels_container,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_transform_pivot_x(labels_container, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_y(labels_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    song_label = lv_label_create(labels_container);
    lv_obj_set_width(song_label, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(song_label, LV_SIZE_CONTENT); /// 1
    lv_label_set_text(song_label, song_name.c_str());
    lv_obj_set_style_text_font(song_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    artist_label = lv_label_create(labels_container);
    lv_obj_set_width(artist_label, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(artist_label, LV_SIZE_CONTENT); /// 1
    lv_label_set_text(artist_label, artist_name.c_str());
    lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    add_to_queue_btn = lv_btn_create(playlist_item_pannel);
    lv_obj_set_width(add_to_queue_btn, 60);
    lv_obj_set_height(add_to_queue_btn, 30);
    lv_obj_set_x(add_to_queue_btn, 121);
    lv_obj_set_y(add_to_queue_btn, -50);
    lv_obj_set_align(add_to_queue_btn, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(add_to_queue_btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(add_to_queue_btn, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(add_to_queue_btn, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(add_to_queue_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(add_to_queue_btn,
                                &ui_img_playlist_add_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(add_to_queue_btn, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(add_to_queue_btn, 150, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(playlist_item_pannel, song_clicked_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(add_to_queue_btn, song_queue_add_clicked_cb, LV_EVENT_CLICKED, this);
    // xSemaphoreGive(ui_sem);
    ui_lvgl_unlock();
}
SpotifyPlaylistItem::~SpotifyPlaylistItem()
{
    ui_lvgl_lock(-1);

    // Remove all event callbacks first
    lv_obj_remove_event_cb(playlist_item_pannel, song_clicked_cb);
    lv_obj_remove_event_cb(add_to_queue_btn, song_queue_add_clicked_cb);
    lv_obj_del(playlist_item_pannel);
    song_name.clear();
    artist_name.clear();

    ui_lvgl_unlock();
}
void SpotifyPlaylistItem::setSongName(const std::string& name)
{
    ui_lvgl_lock(-1);
    song_name = name;
    lv_label_set_text(song_label, song_name.c_str());
    ui_lvgl_unlock();
}
void SpotifyPlaylistItem::setArtistName(const std::string& name)
{
    ui_lvgl_lock(-1);
    artist_name = name;
    lv_label_set_text(artist_label, artist_name.c_str());
    ui_lvgl_unlock();
}
