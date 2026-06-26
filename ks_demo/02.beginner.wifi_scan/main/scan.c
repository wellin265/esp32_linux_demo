/*
 * Public Domain (or CC0 licensed, at your option.)
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "ksdiy_example_display.h"
#include "nvs_flash.h"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE

static const char *TAG = "scan";

static void update_scan_display(const char *line1, const char *line2, const char *line3)
{
    ksdiy_example_display_set_lines(line1, line2, line3);
}

static void wifi_scan(void)
{
    update_scan_display("starting scan...", "", "");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    char line1[64];
    char line2[64];
    char line3[64];
    snprintf(line1, sizeof(line1), "AP found=%u", ap_count);
    if (number > 0) {
        snprintf(line2, sizeof(line2), "%s", ap_info[0].ssid);
        snprintf(line3, sizeof(line3), "RSSI=%d ch=%d", ap_info[0].rssi, ap_info[0].primary);
    } else {
        snprintf(line2, sizeof(line2), "no network");
        snprintf(line3, sizeof(line3), "scan complete");
    }
    update_scan_display(line1, line2, line3);
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("02.beginner.wifi_scan", "nearby AP list");
    update_scan_display("init nvs...", "", "");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_scan();
}
