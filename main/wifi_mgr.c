#include "wifi_mgr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_mgr";
static EventGroupHandle_t s_wifi_events;
#define CONNECTED_BIT BIT0
static bool s_connected = false;
static bool s_inited = false;
static esp_netif_t *s_netif = NULL;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED) {
            if (id == WIFI_EVENT_STA_DISCONNECTED) {
                s_connected = false;
                ESP_LOGW(TAG, "Disconnected, reconnecting...");
            }
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        xEventGroupSetBits(s_wifi_events, CONNECTED_BIT);
    }
}

void wifi_mgr_init(const char *ssid, const char *pass) {
    s_wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta();

    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Free internal RAM: %u bytes", free_internal);

    // Reduced internal RAM usage — need enough for stable WPA handshake
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 4;       // was 2, need more for WPA
    cfg.dynamic_rx_buf_num = 8;      // was 4
    cfg.tx_buf_type = 1;             // dynamic TX
    cfg.static_tx_buf_num = 0;
    cfg.dynamic_tx_buf_num = 8;      // was 4
    cfg.ampdu_rx_enable = 0;
    cfg.ampdu_tx_enable = 0;
    cfg.nvs_enable = 1;

    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s (need more internal RAM)", esp_err_to_name(err));
        return;
    }

    s_inited = true;
    ESP_LOGI(TAG, "WiFi initialized, internal RAM remaining: %u",
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to %s...", ssid);
}

bool wifi_mgr_wait(int timeout_ms) {
    if (!s_inited) return false;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & CONNECTED_BIT) != 0;
}

bool wifi_mgr_is_connected(void) {
    return s_connected;
}

void wifi_mgr_get_ip(char *buf, int buf_len) {
    esp_netif_ip_info_t ip_info;
    if (s_netif && esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK) {
        snprintf(buf, buf_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(buf, buf_len, "0.0.0.0");
    }
}
