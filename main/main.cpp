/*
 * Project: MBeacon Clone (ESP-IDF Pure C)
 * Device: ESP32
 */

#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

static char *TAG = "IBEACON_EMU";

// Advertising Data (Flags + iBeacon Manufacturer Data) = 30 bytes
static uint8_t adv_raw[] = {
  0x02, 0x01, 0x06,                    // Flags: 0x06
  0x1A, 0xFF,                          // Len=0x1A, Type=0xFF (Manufacturer)
  0x4C, 0x00,                          // Apple Company ID (0x004C LE)
  0x02, 0x15,                          // iBeacon type=0x02, len=0x15
  // UUID e2c56db5-dffb-48d2-b060-d0f5a71096e0
  0xE2,0xC5,0x6D,0xB5,0xDF,0xFB,0x48,0xD2,0xB0,0x60,0xD0,0xF5,0xA7,0x10,0x96,0xE0,
  0x9C, 0x4B,                          // Major 40011 (0x9C4B)
  0xE0, 0xFD,                          // Minor 57597 (0xE0FD)
  0xC5                                 // Tx Power at 1m = -59 dBm
};

// Scan Response (Tx Power + 0xFFF1 UUID list + Service Data + Complete Local Name) = 31 bytes
static uint8_t scan_rsp_raw[] = {
  0x02, 0x0A, 0x00,                    // Tx Power Level: 0dBm
  0x03, 0x03, 0xF1, 0xFF,              // Complete List of 16-bit UUIDs: 0xFFF1
  0x0E, 0x16, 0xF1, 0xFF,              // Service Data (UUID=0xFFF1)
  0x64, 0x9C, 0x4B, 0xE0, 0xFD, 0xC3, 0x00, 0x00, 0x1C, 0x5F, 0xDF, // payload(11)
  0x08, 0x09,                          // Complete Local Name
  'M','B','e','a','c','o','n'
};

static bool s_adv_raw_set = false;
static bool s_scan_rsp_raw_set = false;

static void start_advertising()
{
    // Legacy 광고(ADV_IND): connectable + scannable
    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min       = 0x00A0; // 100 ms (0.625ms unit) -> 160 * 0.625ms
    adv_params.adv_int_max       = 0x00A0; // 고정
    adv_params.adv_type          = ADV_TYPE_IND;
    adv_params.own_addr_type     = BLE_ADDR_TYPE_RANDOM; // 아래에서 rand addr 고정
    adv_params.channel_map       = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&adv_params));
    ESP_LOGI(TAG, "Advertising started.");
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "ADV raw set complete");
        s_adv_raw_set = true;
        if (s_adv_raw_set && s_scan_rsp_raw_set) start_advertising();
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "ScanRsp raw set complete");
        s_scan_rsp_raw_set = true;
        if (s_adv_raw_set && s_scan_rsp_raw_set) start_advertising();
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "ADV start success");
        } else {
            ESP_LOGE(TAG, "ADV start failed, status=%d", param->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ADV stopped, status=%d", param->adv_stop_cmpl.status);
        break;

    default:
        break;
    }
}

extern "C" void app_main(void)
{
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // BT controller
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));

    // === MAC(Address) 고정: Random Static Address ===
    // 표시 주소: C3:00:00:1C:5F:DF
    uint8_t rand_addr[6] = {0xC3, 0x00, 0x00, 0x1C, 0x5F, 0xDF};
    ESP_ERROR_CHECK(esp_ble_gap_set_rand_addr(rand_addr));

    // Raw ADV / ScanRsp 세팅
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv_raw, sizeof(adv_raw)));
    ESP_ERROR_CHECK(esp_ble_gap_config_scan_rsp_data_raw(scan_rsp_raw, sizeof(scan_rsp_raw)));

}