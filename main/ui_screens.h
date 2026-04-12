#pragma once

#include "lvgl.h"
#include "app_state.h"

// Screen management
typedef enum { SCREEN_DASH = 0, SCREEN_ACTIVITY, SCREEN_SEND } screen_t;

void ui_build_all(void);
void ui_switch_to(screen_t screen);

// Dashboard updates
void dash_update_time(const char *time_str);
void dash_update_health(bool gw_online, uint32_t last_fetch_age_s, const char *model);
void dash_update_activity(agent_status_t status, const char *session_label, uint32_t duration_s, uint8_t context_pct, uint32_t context_used, uint32_t context_max);
void dash_update_cost(float cost_today, float rate_per_hour);
void dash_update_tokens(uint32_t tokens_today);
void dash_update_sessions(uint8_t active, uint8_t total);
void dash_update_info(uint8_t budget_window_pct, const char *window_label, uint8_t budget_week_pct, uint8_t cache_hit_pct);
void dash_update_alerts(const app_state_t *state, uint32_t now_ms);

// Activity screen
void activity_update(const app_state_t *state);

// Control screen
void send_update_response(const char *text, bool loading);

// Inter-screen communication
extern volatile int pending_command;
