#include "wifi.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <ssid_manager.h>
#include <wifi_manager.h>

#define TAG "wifi_task"

void wifiTask(void *pvParameters) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  auto &wifi_manager = WifiManager::GetInstance();
  // Initialize with configuration
  WifiManagerConfig config;
  config.ssid_prefix = "ESP32"; // AP mode SSID prefix
  config.language = "zh-CN";    // Web UI language

  // Optional: tune connection-to-strongest-AP behavior
  // config.station_failure_retry_cnt = 3;  // retries before falling back to
  // weaker AP

  // Optional: show advanced fields in the config portal
  // config.show_ota_config = true;    // show Custom OTA URL input
  // config.show_sleep_config = true;  // show Sleep Mode toggle

  wifi_manager.Initialize(config);

  // Set event callback to handle WiFi events
  // The callback receives the event type and optional data (e.g., disconnect
  // reason)
  wifi_manager.SetEventCallback([](WifiEvent event, const std::string &data) {
    switch (event) {
    case WifiEvent::Scanning:
      ESP_LOGI("WiFi", "Scanning for networks...");
      break;
    case WifiEvent::Connecting:
      ESP_LOGI("WiFi", "Connecting to network...");
      break;
    case WifiEvent::Connected:
      ESP_LOGI("WiFi", "Connected successfully!");
      break;
    case WifiEvent::Disconnected:
      // data contains the disconnect reason code
      ESP_LOGW("WiFi", "Disconnected from network, reason: %s", data.c_str());
      break;
    case WifiEvent::ConfigModeEnter:
      ESP_LOGI("WiFi", "Entered config mode");
      break;
    case WifiEvent::ConfigModeExit:
      ESP_LOGI("WiFi", "Exited config mode");
      break;
    }
  });

  // Check if there are saved Wi-Fi credentials
  auto &ssid_list = SsidManager::GetInstance().GetSsidList();
  if (ssid_list.empty()) {
    // No credentials saved, start config AP mode
    wifi_manager.StartConfigAp();
  } else {
    // Try to connect to the saved Wi-Fi network
    wifi_manager.StartStation();
  }
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}