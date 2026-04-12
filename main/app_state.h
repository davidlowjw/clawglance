#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"

// ============================================================
// ClawGlance Data Models
// ============================================================

typedef enum {
    AGENT_UNKNOWN = 0,
    AGENT_IDLE,
    AGENT_ACTIVE,
    AGENT_ERROR,
    AGENT_DISCONNECTED
} agent_status_t;

typedef enum {
    CONN_GATEWAY_CONNECTED,
    CONN_GATEWAY_ERROR,
    CONN_DISCONNECTED
} connection_state_t;

typedef struct {
    char session_key[48];
    char agent_id[16];
    char label[32];
    agent_status_t status;
    uint32_t token_count;
    float cost_usd;
    bool is_active;
    char last_message[128];
    uint32_t last_active;
} session_info_t;

typedef struct {
    connection_state_t connection;
    bool healthy;
    char version[24];
    uint8_t active_session_count;
    uint8_t total_session_count;
    float cost_today;
    uint32_t tokens_today;
    char current_model[48];
} gateway_state_t;

typedef struct {
    uint8_t context_pct;
    char label[16];
} session_context_t;

typedef struct {
    char model[48];
    uint32_t context_max;
    uint32_t context_used;
    uint8_t budget_window_pct;
    char budget_window_label[8];  // "5h"
    uint8_t budget_week_pct;
    uint32_t input_tokens;
    uint32_t output_tokens;
    uint32_t cache_read_tokens;
    uint32_t cache_write_tokens;
    uint8_t cache_hit_pct;
    char active_session_label[24];
    uint32_t active_session_age_s;
    session_context_t sess_ctx[CG_SESSIONS_MAX];
    uint8_t sess_ctx_count;
} telemetry_t;

typedef struct {
    gateway_state_t gateway;
    telemetry_t telemetry;
    session_info_t sessions[CG_SESSIONS_MAX];
    uint8_t session_count;
    char last_response[512];
    uint32_t last_poll_time;
    uint32_t last_health_time;
    char time_string[24];
    uint32_t last_session_fetch;
    // Activity feed (last 4 gateway events)
    struct {
        char ts[6];
        char level[6];
        char msg[72];
    } activity[4];
    uint8_t activity_count;
    // Transcript feed (last 10 session events)
    struct {
        char ts[6];       // "14:27"
        char type[6];     // "user"/"tool"/"reply"
        char text[64];
    } transcript[10];
    uint8_t transcript_count;
    // Cost rate tracking
    float cost_snapshot;
    uint32_t cost_snapshot_time;
    float cost_rate_per_hour;
} app_state_t;

static inline const char* status_to_string(agent_status_t s) {
    switch (s) {
        case AGENT_IDLE:         return "Idle";
        case AGENT_ACTIVE:       return "Active";
        case AGENT_ERROR:        return "Error";
        case AGENT_DISCONNECTED: return "Offline";
        default:                 return "Unknown";
    }
}

static inline uint32_t status_to_color(agent_status_t s) {
    switch (s) {
        case AGENT_IDLE:    return 0x4CAF50;
        case AGENT_ACTIVE:  return 0x2196F3;
        case AGENT_ERROR:   return 0xF44336;
        default:            return 0x9E9E9E;
    }
}
