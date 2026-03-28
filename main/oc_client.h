#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_state.h"

void oc_client_init(const char *host, uint16_t port, const char *token, uint16_t dash_port);
bool oc_check_health(void);
bool oc_check_dashboard(void);
bool oc_fetch_sessions(session_info_t *sessions, uint8_t *count, uint8_t max_count);
bool oc_fetch_costs(float *cost_today, uint32_t *tokens_today);
bool oc_fetch_system_info(gateway_state_t *state);
bool oc_fetch_telemetry(telemetry_t *telemetry);
bool oc_fetch_activity(app_state_t *state);
bool oc_fetch_transcript(app_state_t *state);
bool oc_send_chat(const char *message, char *response, int response_len);
