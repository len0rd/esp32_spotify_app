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
#include <atomic>

static std::atomic<int> scroll_accumulator{0};

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
        ui_lvgl_lock(-1);
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
        {
            lv_label_set_text(ui_Song_Label,
                              sp.getCurrentlyPlayingInfo().currentTrack.name.c_str());
        }

        if (std::string(lv_label_get_text(ui_Artist_Label)) !=
            sp.getCurrentlyPlayingInfo().currentTrack.artist)
        {
            lv_label_set_text(ui_Artist_Label,
                              sp.getCurrentlyPlayingInfo().currentTrack.artist.c_str());
        }

        static bool prev_wifi_connected = false; // default to disconnected
        if (is_wifi_connected() && !prev_wifi_connected)
        {
            lv_img_set_src(ui_Wifi_Indicator,
                           &ui_img_wifi_30dp_e3e3e3_fill0_wght400_grad0_opsz24_png);
            prev_wifi_connected = true;
        }
        else if (!is_wifi_connected() && prev_wifi_connected)
        {
            lv_img_set_src(ui_Wifi_Indicator,
                           &ui_img_wifi_off_30dp_e3e3e3_fill0_wght400_grad0_opsz24_png);
            prev_wifi_connected = false;
        }

        // Apply scoll from encoder
        lv_obj_t* act_scr = lv_scr_act();
        if (scroll_accumulator != 0)
        {
            int scroll_amount  = scroll_accumulator.load();
            scroll_accumulator = 0;

            if (act_scr == ui_Playlists_Screen)
                lv_obj_scroll_by(ui_Playlists_Container, 0, scroll_amount, LV_ANIM_ON);
            else if (act_scr == ui_PlayList_Screen)
                lv_obj_scroll_by(ui_Songs_Container, 0, scroll_amount, LV_ANIM_ON);
            else if (act_scr == ui_Queue_Screen)
                lv_obj_scroll_by(ui_Queue_Container, 0, scroll_amount, LV_ANIM_ON);
        }

        ui_lvgl_unlock();
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

        // get active screen lvgl object
        lv_obj_t* act_scr = lv_scr_act();

        // counter-clockwise encoder tick
        if (READ_BIT(even, 0))
        {
            // change volume only on Now Playing screen
            if (is_wifi_connected() && (act_scr == ui_Now_Playing_Screen))
            {
                sp.changeVolume(-2);
            }
            else if (act_scr == ui_Playlists_Screen || act_scr == ui_PlayList_Screen ||
                     act_scr == ui_Queue_Screen)
            {
                scroll_accumulator += 50;
            }
        }

        // clockwise encoder tick
        if (READ_BIT(even, 1))
        {
            // change volume only on Now Playing screen
            if (is_wifi_connected() && (act_scr == ui_Now_Playing_Screen))
            {
                sp.changeVolume(2);
            }
            else if (act_scr == ui_Playlists_Screen || act_scr == ui_PlayList_Screen ||
                     act_scr == ui_Queue_Screen)
            {
                scroll_accumulator -= 50;
            }
        }
    }
}

extern "C" void app_main(void)
{
    ConsoleCommandsInit();
    espwifi_Init();
    ESP_LOGI(TAG, "Starting Spotify App\n");

    display_init();
    ui_init();

    // remove dummy list items created by ui generator
    ui_lvgl_lock(-1);
    lv_obj_del(ui_Queue_Item_Panel);
    lv_obj_remove_event_cb(ui_Playlist_Panel, ui_event_Playlist_Panel);
    lv_obj_del(ui_Playlist_Panel);
    lv_obj_del(ui_Playlist_Item_Panel);

    // adjust all scrollable containers to start at top
    lv_obj_scroll_by(ui_Playlists_Container, 0, -100, LV_ANIM_OFF);
    lv_obj_scroll_by(ui_Songs_Container, 0, -100, LV_ANIM_OFF);
    lv_obj_scroll_by(ui_Queue_Container, 0, -100, LV_ANIM_OFF);
    ui_lvgl_unlock();

    // add event callbacks
    lv_obj_add_event_cb(ui_Play_Button, ui_play_pause_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Skip_Forward_Btn, ui_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Skip_Back_Btn, ui_previous_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Shuffle_Btn, ui_shuffle_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Now_Playing_Arc, ui_arc_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Now_Playing_Arc, ui_arc_cb, LV_EVENT_RELEASED, NULL);

    // initialize user encoder
    user_encoder_init();
    xTaskCreate(ui_update_task, "ui_update_task", 8 * 1024, NULL, 5, NULL);
    xTaskCreate(user_encoder_loop_task, "user_encoder_loop_task", 8 * 1024, NULL, 2, NULL);

    params::ParamMgr::getInstance().listAll();

    // wait for wifi to connect
    while (!is_wifi_connected())
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // start spotify
    sp.start_task();
    sp.updateCurrentlyPlaying();
    sp.updatePlaybackState();
    sp.requestQueue();
    sp.requestPlaylists();

    while (1)
        vTaskSuspend(NULL);
}
