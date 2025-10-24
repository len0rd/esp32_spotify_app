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

std::string ms_to_min_sec(int ms)
{
    int  total_seconds = ms / 1000;
    int  minutes       = total_seconds / 60;
    int  seconds       = total_seconds % 60;
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    return std::string(buffer);
}

const size_t ARC_MAX_VAL       = 1000;
static bool  arc_being_touched = false;
static void  ui_update_task(void* arg)
{
    lv_arc_set_value(ui_Now_Playing_Arc, 0);

    while (!is_wifi_connected())
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Spotify& sp = Spotify::getInstance();
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
                if (arc_value != lv_arc_get_value(ui_Now_Playing_Arc))
                    lv_arc_set_value(ui_Now_Playing_Arc, arc_value);

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
    Spotify& sp = Spotify::getInstance();
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
        Spotify::getInstance().next();
    }
}

static void ui_previous_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().previous();
    }
}

static void ui_shuffle_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().toggleShuffle();
    }
}

static void ui_arc_cb(lv_event_t* e)
{
    lv_event_code_t code      = lv_event_get_code(e);
    int             arc_value = lv_arc_get_value(ui_Now_Playing_Arc);
    int             position_ms =
        (int) ((float) arc_value / (float) ARC_MAX_VAL *
               (float) Spotify::getInstance().getCurrentlyPlayingInfo().currentTrack.duration_ms);
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
        Spotify::getInstance().seek(position_ms);

        arc_being_touched = false;
    }
}

static void user_encoder_loop_task(void* arg)
{
    Spotify& sp = Spotify::getInstance();

    for (;;)
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

extern "C" void app_main(void)
{
    Spotify::getInstance();
    ConsoleCommandsInit();
    espwifi_Init();
    ESP_LOGI(TAG, "Starting Spotify App\n");

    display_init();
    ui_init();
    lv_obj_add_event_cb(ui_Play_Button, ui_play_pause_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Skip_Forward_Btn, ui_next_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Skip_Back_Btn, ui_previous_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Shuffle_Btn, ui_shuffle_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Now_Playing_Arc, ui_arc_cb, LV_EVENT_ALL, NULL);
    user_encoder_init();
    xTaskCreate(ui_update_task, "ui_update_task", 8 * 1024, NULL, 5, NULL);

    params::ParamMgr::getInstance().listAll();

    // wait for wifi to connect
    while (!is_wifi_connected())
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(user_encoder_loop_task, "user_encoder_loop_task", 8 * 1024, NULL, 2, NULL);
    Spotify::getInstance().start_task();
    Spotify::getInstance().updateCurrentlyPlaying();
    Spotify::getInstance().updatePlaybackState();

    while (1)
        vTaskSuspend(NULL);
}
