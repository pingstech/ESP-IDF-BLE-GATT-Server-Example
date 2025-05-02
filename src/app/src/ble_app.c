/**
 * @file ble_app.c
 * @author Furkan YAYLA
 * @brief 
 * @version 0.1
 * @date 2025-04-18
 * 
 * @copyright Copyright (c) 2025
 * 
 */

// Standard Library Includes
#include <stdio.h>
#include <string.h>

/* Kernel Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Platform Includes */
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* Driver Layer Includes */
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "cJSON.h"

/* Application Layer Includes */
#include "ble_app.h"

#define TAG "BLE"
#define DEVICE_NAME "BLE_DEMO"

static uint16_t gl_service_handle;
static uint16_t gl_char_handle;
static uint16_t gl_conn_id;

static uint8_t service_uuid[16] = 
{
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 00, 0x00, 0x00, 0x00, 0x01
};

static esp_ble_adv_data_t adv_data = 
{
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising started");
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising stopped");
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GATTS_REG_EVT:
        {
            esp_ble_gap_set_device_name(DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);

            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id.inst_id = 0x00,
                .id.uuid.len = ESP_UUID_LEN_128
            };
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, 16);

            esp_ble_gatts_create_service(gatts_if, &service_id, 4);
            break;
        }

        case ESP_GATTS_CREATE_EVT:
        {
            gl_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(gl_service_handle);

            esp_bt_uuid_t char_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = 0xFFE1},
            };

            esp_gatt_char_prop_t prop = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE;

            esp_ble_gatts_add_char(gl_service_handle, &char_uuid,
                                   ESP_GATT_PERM_WRITE,
                                   prop,
                                   NULL, NULL);
            break;
        }

        case ESP_GATTS_ADD_CHAR_EVT:
        {
            gl_char_handle = param->add_char.attr_handle;

            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG},  // UUID 0x2902
            };

            // CCCD read + write izni ile ekleniyor
            esp_ble_gatts_add_char_descr(gl_service_handle,
                                         &descr_uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         NULL, NULL);
            break;
        }

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        {
            ESP_LOGI(TAG, "Descriptor added. Handle: %d", param->add_char_descr.attr_handle);
            break;
        }

        case ESP_GATTS_CONNECT_EVT:
        {
            ESP_LOGI(TAG, "Device connected, conn_id=%d", param->connect.conn_id);
            gl_conn_id = param->connect.conn_id;
            break;
        }

        case ESP_GATTS_WRITE_EVT:
        {
            ESP_LOGI(TAG, "Write event, handle=%d, len=%d", param->write.handle, param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);

            // CCCD enable kontrolü
            if (param->write.len == 2 && param->write.value)
            {
                uint16_t descr_val = param->write.value[1] << 8 | param->write.value[0];
                if (descr_val == 0x0002) {
                    ESP_LOGI(TAG, "Indication enabled by client.");
                } else if (descr_val == 0x0000) {
                    ESP_LOGI(TAG, "Indication disabled by client.");
                }
            }

            // JSON mesajı parse ve cevap gönderme
            if (param->write.len < 200) 
            {
                char json_buf[201] = {0};
                memcpy(json_buf, param->write.value, param->write.len);
                json_buf[param->write.len] = '\0';

                cJSON *root = cJSON_Parse(json_buf);
                if (root) 
                {
                    cJSON *msg_item = cJSON_GetObjectItem(root, "message");
                    if (cJSON_IsString(msg_item)) 
                    {
                        char response[128];
                        snprintf(response, sizeof(response), "{\"ack\":\"%s\"}", msg_item->valuestring);

                        // Indication gönderimi
                        esp_ble_gatts_send_indicate(
                            gatts_if,
                            param->write.conn_id,
                            gl_char_handle,
                            strlen(response),
                            (uint8_t *)response,
                            true // confirmation isteği için true
                        );
                    } 
                    
                    else 
                    {
                        const char *err = "{\"error\":\"missing_message\"}";
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
                                                    gl_char_handle, strlen(err), (uint8_t *)err, true);
                    }
                    cJSON_Delete(root);
                } 
                
                else 
                {
                    const char *err = "{\"error\":\"invalid_json\"}";
                    ESP_LOGW(TAG, "Invalid JSON received: %s", json_buf);  // Loglama eklendi
                    esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
                                                gl_char_handle, strlen(err), (uint8_t *)err, true);
                }
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(
                    gatts_if,
                    param->write.conn_id,
                    param->write.trans_id,
                    ESP_GATT_OK, NULL);
            }
            break;
        }

        case ESP_GATTS_DISCONNECT_EVT:
        {
            ESP_LOGI(TAG, "Device disconnected");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        }

        default:
            break;
    }
}

esp_err_t ble_app_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    return ESP_OK;
}