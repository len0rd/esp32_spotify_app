/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"

#include "user_config.h"
#include "user_encoder_bsp.h"
#include "ui.h"
#include "display_init.h"

#include "ParamMgr.h"
#include "ConsoleCommands.h"
#include "wifi.h"
#include <Spotify.hpp>

static const char* TAG = "main";
static Spotify&    sp  = Spotify::getInstance();

std::string ms_to_min_sec(int ms)
{
    int total_seconds = ms / 1000;
    int minutes       = total_seconds / 60;
    int seconds       = total_seconds % 60;
    return std::to_string(minutes) + ":" + std::to_string(seconds) + (seconds < 10 ? "0" : "");
}

const size_t ARC_MAX_VAL       = 1000;
static bool  arc_being_touched = false;
static void  ui_update_task(void* arg)
{
    lv_arc_set_value(ui_Now_Playing_Arc, 0);

    while (!is_wifi_connected())
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while (1)
    {
        if (sp.isPlaying())
        {
            // set play button state to checked to display pause icon
            lv_obj_add_state(ui_Play_Button, LV_STATE_CHECKED);

            // update arc
            if (!arc_being_touched)
            {
                int arc_value =
                    (int) (ARC_MAX_VAL * sp.getCurrentlyPlayingInfo().getProgress_percent());
                int last_arc_value = lv_arc_get_value(ui_Now_Playing_Arc);
                if (arc_value != last_arc_value)
                {
                    lv_arc_set_value(ui_Now_Playing_Arc, arc_value);
                    int diff = abs(arc_value - last_arc_value);
                    if (diff > 20)
                    {
                        lv_obj_invalidate(ui_Now_Playing_Arc); // force redraw for large jumps
                    }
                }

                std::string track_progress =
                    ms_to_min_sec(sp.getCurrentlyPlayingInfo().getProgress_ms());
                if (std::string(lv_label_get_text(ui_Song_Time_Played_Label)) != track_progress)
                    lv_label_set_text(ui_Song_Time_Played_Label, track_progress.c_str());
            }

            std::string track_duration =
                ms_to_min_sec(sp.getCurrentlyPlayingInfo().currentTrack.duration_ms);
            if (std::string(lv_label_get_text(ui_Song_Time_Remaining_Label)) != track_duration)
                lv_label_set_text(ui_Song_Time_Remaining_Label, track_duration.c_str());
        }
        else
        {
            // set play button state to normal to display play icon
            lv_obj_clear_state(ui_Play_Button, LV_STATE_CHECKED);
        }

        if (sp.getPlaybackState().shuffle_state)
        {
            lv_obj_add_state(ui_Shuffle_Btn, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui_Shuffle_Btn, LV_STATE_CHECKED);
        }

        if (std::string(lv_label_get_text(ui_Song_Label)) !=
            sp.getCurrentlyPlayingInfo().currentTrack.name)
            lv_label_set_text(ui_Song_Label,
                              sp.getCurrentlyPlayingInfo().currentTrack.name.c_str());

        if (std::string(lv_label_get_text(ui_Artist_Label)) !=
            sp.getCurrentlyPlayingInfo().currentTrack.artist)
            lv_label_set_text(ui_Artist_Label,
                              sp.getCurrentlyPlayingInfo().currentTrack.artist.c_str());

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void ui_play_pause_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;
    if (sp.isPlaying())
    {
        sp.pause();
    }
    else
    {
        sp.play();
    }
}

static void ui_next_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        sp.next();
    }
}

static void ui_previous_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        sp.previous();
    }
}

static void ui_shuffle_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        sp.toggleShuffle();
    }
}

static void ui_arc_cb(lv_event_t* e)
{
    lv_event_code_t code      = lv_event_get_code(e);
    int             arc_value = lv_arc_get_value(ui_Now_Playing_Arc);
    int             position_ms =
        (arc_value * sp.getCurrentlyPlayingInfo().currentTrack.duration_ms) / ARC_MAX_VAL;
    // update time label while dragging
    std::string track_progress = ms_to_min_sec(position_ms);
    if (std::string(lv_label_get_text(ui_Song_Time_Played_Label)) != track_progress)
        lv_label_set_text(ui_Song_Time_Played_Label, track_progress.c_str());

    if (code == LV_EVENT_PRESSED)
    {
        arc_being_touched = true;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        sp.seek(position_ms);

        arc_being_touched = false;
    }
}

static void user_encoder_loop_task(void* arg)
{
    while (1)
    {
        EventBits_t even =
            xEventGroupWaitBits(knob_even_, BIT_EVEN_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

        // counter-clockwise encoder tick
        if (READ_BIT(even, 0))
        {
            sp.setVolume(std::max(0, sp.getPlaybackState().volume_percent - 5));
        }

        // clockwise encoder tick
        if (READ_BIT(even, 1))
        {
            sp.setVolume(std::min(100, sp.getPlaybackState().volume_percent + 5));
        }
    }
}

class SpotifyQueueItem
{
public:
    static void song_queue_clicked_cb(lv_event_t* e)
    {
        SpotifyQueueItem* data = static_cast<SpotifyQueueItem*>(lv_event_get_user_data(e));
        lv_event_code_t   code = lv_event_get_code(e);
        if (data && code == LV_EVENT_CLICKED)
        {
            printf("Clicked on song: %s by %s\n", data->song_name.c_str(),
                   data->artist_name.c_str());
        }
    }
    static void song_queue_up_clicked_cb(lv_event_t* e)
    {
        SpotifyQueueItem* data = static_cast<SpotifyQueueItem*>(lv_event_get_user_data(e));
        lv_event_code_t   code = lv_event_get_code(e);
        if (data && code == LV_EVENT_CLICKED)
        {
            printf("Clicked UP BTN for song: %s by %s\n", data->song_name.c_str(),
                   data->artist_name.c_str());
        }
    }
    static void song_queue_down_clicked_cb(lv_event_t* e)
    {
        SpotifyQueueItem* data = static_cast<SpotifyQueueItem*>(lv_event_get_user_data(e));
        lv_event_code_t   code = lv_event_get_code(e);
        if (data && code == LV_EVENT_CLICKED)
        {
            printf("Clicked DOWN BTN for song: %s by %s\n", data->song_name.c_str(),
                   data->artist_name.c_str());
        }
    }

    SpotifyQueueItem() : SpotifyQueueItem("Song Name", "Artist Name") {}
    SpotifyQueueItem(const std::string& song, const std::string& artist)
        : song_name(song), artist_name(artist)
    {
        queue_item_pannel = lv_obj_create(ui_Queue_Container);
        lv_obj_set_width(queue_item_pannel, 360);
        lv_obj_set_height(queue_item_pannel, 45);
        lv_obj_set_align(queue_item_pannel, LV_ALIGN_CENTER);
        lv_obj_set_flex_flow(queue_item_pannel, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(queue_item_pannel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(queue_item_pannel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_bg_color(queue_item_pannel, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(queue_item_pannel, lv_color_hex(0x413C3C),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(queue_item_pannel, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(queue_item_pannel, 45, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(queue_item_pannel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(queue_item_pannel, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(queue_item_pannel, 150, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(queue_item_pannel, song_queue_clicked_cb, LV_EVENT_CLICKED, this);

        labels_container = lv_obj_create(queue_item_pannel);
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
        lv_obj_set_style_text_font(song_label, &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

        artist_label = lv_label_create(labels_container);
        lv_obj_set_width(artist_label, LV_SIZE_CONTENT);  /// 1
        lv_obj_set_height(artist_label, LV_SIZE_CONTENT); /// 1
        lv_label_set_text(artist_label, artist_name.c_str());
        lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_12,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

        move_up_btn = lv_btn_create(queue_item_pannel);
        lv_obj_set_width(move_up_btn, 30);
        lv_obj_set_height(move_up_btn, 30);
        lv_obj_set_x(move_up_btn, 121);
        lv_obj_set_y(move_up_btn, -50);
        lv_obj_set_align(move_up_btn, LV_ALIGN_RIGHT_MID);
        lv_obj_add_flag(move_up_btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(move_up_btn, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
        lv_obj_set_style_bg_color(move_up_btn, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(move_up_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(
            move_up_btn, &ui_img_keyboard_arrow_up_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png,
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(move_up_btn, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(move_up_btn, 150, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(move_up_btn, song_queue_up_clicked_cb, LV_EVENT_CLICKED, this);

        move_down_btn = lv_btn_create(queue_item_pannel);
        lv_obj_set_width(move_down_btn, 30);
        lv_obj_set_height(move_down_btn, 30);
        lv_obj_set_x(move_down_btn, 121);
        lv_obj_set_y(move_down_btn, -50);
        lv_obj_set_align(move_down_btn, LV_ALIGN_RIGHT_MID);
        lv_obj_add_flag(move_down_btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(move_down_btn, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
        lv_obj_set_style_bg_color(move_down_btn, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(move_down_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(
            move_down_btn, &ui_img_keyboard_arrow_down_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png,
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(move_down_btn, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(move_down_btn, 150, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(move_up_btn, song_queue_up_clicked_cb, LV_EVENT_CLICKED, this);
    }
    ~SpotifyQueueItem()
    {
        lv_obj_del(queue_item_pannel);
        song_name.clear();
        artist_name.clear();
    }
    void setSongName(const std::string& name)
    {
        song_name = name;
        lv_label_set_text(song_label, song_name.c_str());
    }
    void setArtistName(const std::string& name)
    {
        artist_name = name;
        lv_label_set_text(artist_label, artist_name.c_str());
    }

    std::string song_name;
    std::string artist_name;
    lv_obj_t*   queue_item_pannel;
    lv_obj_t*   labels_container;
    lv_obj_t*   song_label;
    lv_obj_t*   artist_label;
    lv_obj_t*   move_up_btn;
    lv_obj_t*   move_down_btn;
};

extern "C" void app_main(void)
{
    ConsoleCommandsInit();
    espwifi_Init();
    ESP_LOGI(TAG, "Starting Spotify App\n");

    display_init();
    ui_init();
    lv_obj_add_event_cb(ui_Play_Button, ui_play_pause_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Skip_Forward_Btn, ui_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Skip_Back_Btn, ui_previous_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Shuffle_Btn, ui_shuffle_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Now_Playing_Arc, ui_arc_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Now_Playing_Arc, ui_arc_cb, LV_EVENT_RELEASED, NULL);
    user_encoder_init();
    xTaskCreate(ui_update_task, "ui_update_task", 8 * 1024, NULL, 5, NULL);

    params::ParamMgr::getInstance().listAll();

    // wait for wifi to connect
    while (!is_wifi_connected())
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(user_encoder_loop_task, "user_encoder_loop_task", 8 * 1024, NULL, 2, NULL);
    sp.start_task();
    sp.updateCurrentlyPlaying();
    sp.updatePlaybackState();

    // test create queue objects
    std::vector<std::unique_ptr<SpotifyQueueItem>> queue_items;
    for (int i = 0; i < 10; i++)
    {
        queue_items.push_back(std::make_unique<SpotifyQueueItem>());
        queue_items[i]->setSongName("Song " + std::to_string(i + 1));
        queue_items[i]->setArtistName("Artist " + std::to_string(i + 1));
    }

    // delete item 6 from the queue after 10 seconds
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    printf("Deleting item 6 from queue\n");
    queue_items.erase(queue_items.begin() + 5);

    while (1)
        vTaskSuspend(NULL);
}
