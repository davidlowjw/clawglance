#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_fs.h"
#include "bsp_board.h"
#include "lvgl_init.h"

#include "config.h"
#include "app_state.h"
#include "wifi_mgr.h"
#include "oc_client.h"
#include "ui_screens.h"

static const char *TAG = "clawglance";

static app_state_t app;

// ---- Helpers ----
static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void get_time_string(char *buf, int len) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < 100) {
        snprintf(buf, len, "--:--");
        return;
    }
    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    int hour = timeinfo.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char *ampm = timeinfo.tm_hour >= 12 ? "PM" : "AM";
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%d %s  %d:%02d%s",
        timeinfo.tm_mday, months[timeinfo.tm_mon], hour, timeinfo.tm_min, ampm);
    strlcpy(buf, tmp, len);
}

static uint8_t count_active_sessions(void) {
    uint8_t active = 0;
    for (uint8_t i = 0; i < app.session_count; i++) {
        if (app.sessions[i].is_active) active++;
    }
    return active;
}

static void initial_data_fetch(void) {

    if (oc_fetch_sessions(app.sessions, &app.session_count, CG_SESSIONS_MAX)) {
        app.last_session_fetch = now_ms();
        uint8_t active = count_active_sessions();
        app.gateway.active_session_count = active;
        app.gateway.total_session_count = app.session_count;

    }

    float cost = 0;
    uint32_t tokens = 0;
    if (oc_fetch_costs(&cost, &tokens)) {
        app.gateway.cost_today = cost;
        app.gateway.tokens_today = tokens;
    }

    oc_fetch_system_info(&app.gateway);
    oc_fetch_telemetry(&app.telemetry);
            oc_fetch_activity(&app);
            oc_fetch_transcript(&app);
}

// ---- NTP ----
static void setup_ntp(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CG_NTP_SERVER);
    esp_sntp_init();
    setenv("TZ", "SGT-8", 1);
    tzset();
    ESP_LOGI(TAG, "NTP started");
}


// Pending command execution state
static volatile int cmd_to_run = -1;

// ---- Main task: fetch + update display ----
static void main_task(void *arg) {
    int tick = 0;
    uint32_t last_fetch = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        tick++;
        uint32_t t = now_ms();

        // Gateway health check every 30s (direct to gateway, not via bridge)
        if (wifi_mgr_is_connected() && (t - app.last_health_time > 30000)) {
            app.last_health_time = t;
            app.gateway.healthy = oc_check_health();
            if (app.gateway.healthy)
                app.gateway.connection = CONN_GATEWAY_CONNECTED;
            else
                app.gateway.connection = CONN_GATEWAY_ERROR;
        }

        // Fetch from bridge every 5s
        if (wifi_mgr_is_connected() && (t - last_fetch > CG_POLL_INTERVAL_MS)) {
            last_fetch = t;

            if (oc_fetch_sessions(app.sessions, &app.session_count, CG_SESSIONS_MAX)) {
                app.last_session_fetch = t;
            }
            app.gateway.active_session_count = count_active_sessions();
            app.gateway.total_session_count = app.session_count;

            float cost = 0;
            uint32_t tokens = 0;
            if (oc_fetch_costs(&cost, &tokens)) {
                app.gateway.cost_today = cost;
                app.gateway.tokens_today = tokens;
            }

            oc_fetch_system_info(&app.gateway);
            oc_fetch_telemetry(&app.telemetry);
            oc_fetch_activity(&app);
            oc_fetch_transcript(&app);
        }

        // Pick up command request — show loading state first
        if (pending_command >= 0 && wifi_mgr_is_connected()) {
            cmd_to_run = pending_command;
            pending_command = -1;
            const char *labels[] = { "Restarting gateway...", "Fetching status..." };
            if (cmd_to_run >= 0 && cmd_to_run <= 1) {
                lv_port_sem_take();
                send_update_response(labels[cmd_to_run], true);
                lv_port_sem_give();
            }
        }

        // Cost rate calculation (every 5 minutes)
        if (app.cost_snapshot_time == 0) {
            app.cost_snapshot = app.gateway.cost_today;
            app.cost_snapshot_time = t;
        } else if (t - app.cost_snapshot_time > 300000) {
            float delta = app.gateway.cost_today - app.cost_snapshot;
            float hours = (t - app.cost_snapshot_time) / 3600000.0f;
            if (hours > 0) app.cost_rate_per_hour = delta / hours;
            app.cost_snapshot = app.gateway.cost_today;
            app.cost_snapshot_time = t;
        }

        // Determine agent status from telemetry
        agent_status_t agent_status = AGENT_IDLE;
        if (app.telemetry.active_session_age_s < 120) agent_status = AGENT_ACTIVE;
        if (!app.gateway.healthy) agent_status = AGENT_DISCONNECTED;

        // Context percentage
        uint8_t ctx_pct = 0;
        if (app.telemetry.context_max > 0)
            ctx_pct = (uint8_t)((uint64_t)app.telemetry.context_used * 100 / app.telemetry.context_max);

        // Health indicators
        bool bridge_ok = (app.last_session_fetch > 0 && (t - app.last_session_fetch) < 30000);
        uint32_t fetch_age = app.last_session_fetch > 0 ? (t - app.last_session_fetch) / 1000 : 999;

        // Update display every second
        char time_buf[24];
        get_time_string(time_buf, sizeof(time_buf));

        lv_port_sem_take();
        dash_update_time(time_buf);
        dash_update_health(app.gateway.healthy, bridge_ok, fetch_age, app.telemetry.model);
        dash_update_activity(agent_status, app.telemetry.active_session_label, app.telemetry.active_session_age_s, ctx_pct, app.telemetry.context_used, app.telemetry.context_max);
        dash_update_cost(app.gateway.cost_today, app.cost_rate_per_hour);
        dash_update_tokens(app.gateway.tokens_today);
        dash_update_sessions(app.gateway.active_session_count, app.gateway.total_session_count);
        dash_update_info(app.telemetry.budget_window_pct, app.telemetry.budget_window_label, app.telemetry.budget_week_pct, app.telemetry.cache_hit_pct);
        activity_update(&app);
        lv_port_sem_give();

        // Execute command AFTER display update (blocks but display already refreshed)
        if (cmd_to_run >= 0 && cmd_to_run <= 1) {
            int cmd = cmd_to_run;
            cmd_to_run = -1;
            const char *cmd_msgs[] = { "/restart-gateway", "/status" };

            char response[512];
            oc_send_chat(cmd_msgs[cmd], response, sizeof(response));
            if (strlen(response) > 300) response[300] = '\0';
            strlcpy(app.last_response, response, sizeof(app.last_response));

            lv_port_sem_take();
            send_update_response(response, false);
            lv_port_sem_give();
        }
    }
}

// ============================================================
// app_main
// ============================================================
void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ClawGlance v%s", CLAWGLANCE_VERSION);
    ESP_LOGI(TAG, "  ESP32-S3 3.5\" Port");
    ESP_LOGI(TAG, "========================================");

    // NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Board BSP init (LCD, I2C, etc.) — must come first, needs contiguous DMA RAM
    sys_int();

    // LVGL init (display, touch, tick timer, handler task)
    lvgl_init();

    ESP_LOGI(TAG, "Display: %dx%d, Free internal heap: %lu",
        lv_disp_get_default()->driver->hor_res,
        lv_disp_get_default()->driver->ver_res,
        (unsigned long)esp_get_free_internal_heap_size());

    // Load WiFi config from NVS (falls back to compile-time defaults)
    char wifi_ssid[33] = "";
    char wifi_pass[65] = "";
    {
        nvs_handle_t h;
        size_t len;
        if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
            len = sizeof(wifi_ssid);
            nvs_get_str(h, "wifi_ssid", wifi_ssid, &len);
            len = sizeof(wifi_pass);
            nvs_get_str(h, "wifi_pass", wifi_pass, &len);
            nvs_close(h);
        }
        if (wifi_ssid[0] == '\0') strlcpy(wifi_ssid, CG_WIFI_SSID, sizeof(wifi_ssid));
        if (wifi_pass[0] == '\0') strlcpy(wifi_pass, CG_WIFI_PASS, sizeof(wifi_pass));
    }
    ESP_LOGI(TAG, "WiFi SSID: %s", wifi_ssid);

    // Init WiFi BEFORE building UI — UI widgets use PSRAM, but WiFi needs internal RAM
    wifi_mgr_init(wifi_ssid, wifi_pass);

    ESP_LOGI(TAG, "After WiFi init, free internal: %lu",
        (unsigned long)esp_get_free_internal_heap_size());

    // Build UI screens (widgets allocate from PSRAM via malloc)
    lv_port_sem_take();
    ui_build_all();

    lv_port_sem_give();
    bool wifi_ok = wifi_mgr_wait(CG_WIFI_TIMEOUT_MS);

    if (wifi_ok) {
        char ip[20];
        wifi_mgr_get_ip(ip, sizeof(ip));
        ESP_LOGI(TAG, "WiFi connected: %s", ip);

        char msg[64];
        snprintf(msg, sizeof(msg), "WiFi connected: %s", ip);

        // NTP
        setup_ntp();

        // Init OC client
        // Load gateway config from NVS
        char gw_host_port[48] = "";
        char gw_token[65] = "";
        char gw_host[32] = "";
        uint16_t gw_port = CG_OC_PORT;
        {
            nvs_handle_t h;
            size_t len;
            if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
                len = sizeof(gw_host_port);
                nvs_get_str(h, "gw_host", gw_host_port, &len);
                len = sizeof(gw_token);
                nvs_get_str(h, "gw_token", gw_token, &len);
                nvs_close(h);
            }
            if (gw_host_port[0] == '\0') snprintf(gw_host_port, sizeof(gw_host_port), "%s:%d", CG_OC_HOST, CG_OC_PORT);
            if (gw_token[0] == '\0') strlcpy(gw_token, CG_OC_TOKEN, sizeof(gw_token));
            // Parse host:port
            char *colon = strchr(gw_host_port, ':');
            if (colon) {
                int hlen = colon - gw_host_port;
                if (hlen > 0 && hlen < (int)sizeof(gw_host)) {
                    memcpy(gw_host, gw_host_port, hlen);
                    gw_host[hlen] = '\0';
                }
                gw_port = (uint16_t)atoi(colon + 1);
            } else {
                strlcpy(gw_host, gw_host_port, sizeof(gw_host));
            }
        }
        ESP_LOGI(TAG, "Gateway: %s:%d", gw_host, gw_port);
        oc_client_init(gw_host, gw_port, gw_token, CG_OC_DASH_PORT);

        // Health check

        bool healthy = oc_check_health();
        if (healthy) {
            app.gateway.connection = CONN_GATEWAY_CONNECTED;
            app.gateway.healthy = true;


            // Check dashboard
            app.dashboard_available = oc_check_dashboard();
            app.last_dash_check = now_ms();

            // Initial fetch
            initial_data_fetch();

            lv_port_sem_take();
            if (app.dashboard_available) {

            } else {

            }
            lv_port_sem_give();
        } else {
            app.gateway.connection = CONN_GATEWAY_ERROR;
        }
    } else {
        app.gateway.connection = CONN_DISCONNECTED;
    }

    app.last_poll_time = now_ms();
    app.last_health_time = now_ms();

    ESP_LOGI(TAG, "Boot complete. Free heap: %lu", (unsigned long)esp_get_free_heap_size());

    // Run main loop directly on app_main task (has large stack)
    main_task(NULL); // never returns
}
