/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "button.hpp"
#include "OneButton.h"
#include "led.hpp"
#include "buzzer.h"
#include "flight_control.hpp"

#define PIN_INPUT 0

OneButton button(PIN_INPUT, true);
OneButton remote_init(PIN_REMOTE_INIT, true);

uint8_t is_long_press = 0;

void task_button_update(void *pvParameters);
void LongPressStop(void *oneButton);
void DuringLongPress(void *oneButton);
void Click(void *oneButton);
void RemoteInitClick(void *oneButton);

bool init_button(void) {
    // link functions to be called on events.
    button.attachClick(Click, &button);
    remote_init.attachClick(RemoteInitClick, &remote_init);

    button.setLongPressIntervalMs(3000);

    xTaskCreatePinnedToCore(task_button_update,  // 任务函数
                            "TaskButtonUpdate",  // 任务名称
                            1024 * 4,            // 堆栈大小
                            NULL,                // 传递参数
                            0,                   // 任务优先级
                            NULL,                // 任务句柄
                            tskNO_AFFINITY);     // 无关联，不绑定在任何一个核上

    return true;
}

void task_button_update(void *pvParameters) {
    for (;;) {
        button.tick();
        remote_init.tick();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// this function will be called when the button click.
void Click(void *oneButton) {
    buzzer_sound(4000, 1000);
    esp_restart();
}

// Restart and recalibrate when AtomS3R CAM pulses the Grove input low.
// Ignore the remote input outside parking mode to prevent an in-flight restart.
void RemoteInitClick(void *oneButton) {
    if (Mode != PARKING_MODE) return;

    buzzer_sound(4000, 1000);
    esp_restart();
}

// this function will be called when the button is released.
void LongPressStop(void *oneButton) {
    is_long_press = 0;
    esp_restart();
}

// this function will be called when the button is held down.
void DuringLongPress(void *oneButton) {
    if (!is_long_press) {
        is_long_press = 1;
    }
}
