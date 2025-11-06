#ifndef DISPLAY_INIT_H
#define DISPLAY_INIT_H

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool example_lvgl_lock(int timeout_ms);
void example_lvgl_unlock(void);
void display_init(void);

#endif