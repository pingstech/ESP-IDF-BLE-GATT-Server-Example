/**
 * @file app_manager.c
 * @author Furkan YAYLA
 * @brief 
 * @version 0.1
 * @date 2025-03-26
 * 
 * @copyright Copyright (c) 2025
 * 
 */

/* Standart Library Includes*/
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Platform Includes */
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

/* Kernel Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Application Layer Includes */
#include "ble_app.h"

static const char * TAG = "app_manager";


esp_err_t app_task_manager(void)
{
    esp_err_t status = ESP_OK;

    status |= ble_app_init();

    if (status != ESP_OK) 
    {
        ESP_LOGE(TAG, "app_task_manager failed!");
        return ESP_FAIL;
    }

    return ESP_OK;
}