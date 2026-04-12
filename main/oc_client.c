#include "oc_client.h"
#include "config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "oc_client";

static char s_gw_url[128];
static char s_auth_header[256];

// Response buffer for HTTP reads
#define HTTP_BUF_SIZE 4096
static char s_http_buf[HTTP_BUF_SIZE];
static int s_http_buf_len;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (s_http_buf_len + copy >= HTTP_BUF_SIZE) {
            copy = HTTP_BUF_SIZE - s_http_buf_len - 1;
        }
        if (copy > 0) {
            memcpy(s_http_buf + s_http_buf_len, evt->data, copy);
            s_http_buf_len += copy;
        }
    }
    return ESP_OK;
}

static esp_http_client_handle_t make_client(const char *url, int timeout_ms) {
    s_http_buf_len = 0;
    s_http_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .event_handler = http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Authorization", s_auth_header);
    return client;
}

static bool do_get(const char *url, int timeout_ms) {
    esp_http_client_handle_t client = make_client(url, timeout_ms);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    s_http_buf[s_http_buf_len] = '\0';
    return (err == ESP_OK && status >= 200 && status < 300);
}

void oc_client_init(const char *host, uint16_t port, const char *token) {
    const char *scheme = CG_OC_USE_HTTPS ? "https" : "http";
    snprintf(s_gw_url, sizeof(s_gw_url), "%s://%s:%d", scheme, host, port);
    snprintf(s_auth_header, sizeof(s_auth_header), "Bearer %s", token);
    ESP_LOGI(TAG, "Gateway: %s", s_gw_url);
}

bool oc_check_health(void) {
    char url[192];
    snprintf(url, sizeof(url), "%s/health", s_gw_url);
    bool ok = do_get(url, 3000);
    ESP_LOGI(TAG, "Health: %s", ok ? "OK" : "FAIL");
    return ok;
}

bool oc_fetch_sessions(session_info_t *sessions, uint8_t *count, uint8_t max_count) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/sessions", s_gw_url);

    if (!do_get(url, 3000)) {
        ESP_LOGW(TAG, "Fetch sessions failed");
        return false;
    }

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *arr = cJSON_GetObjectItem(root, "data");
    if (!arr || !cJSON_IsArray(arr)) {
        arr = cJSON_IsArray(root) ? root : NULL;
    }
    if (!arr) {
        cJSON_Delete(root);
        return false;
    }

    *count = 0;
    cJSON *sess;
    cJSON_ArrayForEach(sess, arr) {
        if (*count >= max_count) break;
        session_info_t *s = &sessions[*count];
        memset(s, 0, sizeof(*s));

        cJSON *val;
        val = cJSON_GetObjectItem(sess, "session_key");
        if (!val) val = cJSON_GetObjectItem(sess, "sessionKey");
        if (val && val->valuestring) strlcpy(s->session_key, val->valuestring, sizeof(s->session_key));

        val = cJSON_GetObjectItem(sess, "agent_id");
        if (!val) val = cJSON_GetObjectItem(sess, "agentId");
        if (val && val->valuestring) strlcpy(s->agent_id, val->valuestring, sizeof(s->agent_id));

        val = cJSON_GetObjectItem(sess, "label");
        if (!val) val = cJSON_GetObjectItem(sess, "name");
        if (val && val->valuestring) strlcpy(s->label, val->valuestring, sizeof(s->label));

        val = cJSON_GetObjectItem(sess, "last_message");
        if (!val) val = cJSON_GetObjectItem(sess, "lastMessage");
        if (val && val->valuestring) strlcpy(s->last_message, val->valuestring, sizeof(s->last_message));

        val = cJSON_GetObjectItem(sess, "status");
        const char *st = (val && val->valuestring) ? val->valuestring : "unknown";
        if (strcmp(st, "active") == 0 || strcmp(st, "running") == 0) {
            s->status = AGENT_ACTIVE; s->is_active = true;
        } else if (strcmp(st, "idle") == 0 || strcmp(st, "paused") == 0) {
            s->status = AGENT_IDLE;
        } else if (strcmp(st, "error") == 0) {
            s->status = AGENT_ERROR;
        }

        val = cJSON_GetObjectItem(sess, "token_count");
        if (!val) val = cJSON_GetObjectItem(sess, "tokens");
        if (val) s->token_count = (uint32_t)cJSON_GetNumberValue(val);

        val = cJSON_GetObjectItem(sess, "cost");
        if (!val) val = cJSON_GetObjectItem(sess, "cost_usd");
        if (val) s->cost_usd = (float)cJSON_GetNumberValue(val);

        (*count)++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Fetched %d sessions", *count);
    return true;
}

bool oc_fetch_costs(float *cost_today, uint32_t *tokens_today) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/costs", s_gw_url);
    if (!do_get(url, 3000)) {
        ESP_LOGW(TAG, "Costs: HTTP request failed");
        return false;
    }

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data) {
        cJSON *today = cJSON_GetObjectItem(data, "today");
        if (today) { *cost_today = (float)cJSON_GetNumberValue(today); }

        cJSON *tokens = cJSON_GetObjectItem(data, "tokens");
        if (tokens) { *tokens_today = (uint32_t)cJSON_GetNumberValue(tokens); }

        ESP_LOGI(TAG, "Costs: $%.2f, %lu tokens", *cost_today, (unsigned long)*tokens_today);
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Cost today: $%.2f", *cost_today);
    return true;
}

bool oc_fetch_system_info(gateway_state_t *state) {

    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/system", s_gw_url);
    if (!do_get(url, 3000)) return false;

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) data = root;

    cJSON *val;
    val = cJSON_GetObjectItem(data, "version");
    if (val && val->valuestring) strlcpy(state->version, val->valuestring, sizeof(state->version));

    val = cJSON_GetObjectItem(data, "model");
    if (!val) val = cJSON_GetObjectItem(data, "current_model");
    if (val && val->valuestring) strlcpy(state->current_model, val->valuestring, sizeof(state->current_model));

    val = cJSON_GetObjectItem(data, "active_sessions");
    if (!val) val = cJSON_GetObjectItem(data, "activeSessions");
    if (val) state->active_session_count = (uint8_t)cJSON_GetNumberValue(val);

    val = cJSON_GetObjectItem(data, "total_sessions");
    if (!val) val = cJSON_GetObjectItem(data, "totalSessions");
    if (val) state->total_session_count = (uint8_t)cJSON_GetNumberValue(val);

    cJSON_Delete(root);
    return true;
}

bool oc_fetch_telemetry(telemetry_t *t) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/telemetry", s_gw_url);
    if (!do_get(url, 3000)) return false;

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) { cJSON_Delete(root); return false; }

    cJSON *val;
    val = cJSON_GetObjectItem(data, "model");
    if (val && val->valuestring) strlcpy(t->model, val->valuestring, sizeof(t->model));

    val = cJSON_GetObjectItem(data, "context_max");
    if (val) t->context_max = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "context_used");
    if (val) t->context_used = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "budget_window_pct");
    if (val) t->budget_window_pct = (uint8_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "budget_window_label");
    if (val && val->valuestring) strlcpy(t->budget_window_label, val->valuestring, sizeof(t->budget_window_label));
    val = cJSON_GetObjectItem(data, "budget_week_pct");
    if (val) t->budget_week_pct = (uint8_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "input_tokens");
    if (val) t->input_tokens = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "output_tokens");
    if (val) t->output_tokens = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "cache_read_tokens");
    if (val) t->cache_read_tokens = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "cache_write_tokens");
    if (val) t->cache_write_tokens = (uint32_t)cJSON_GetNumberValue(val);
    val = cJSON_GetObjectItem(data, "cache_hit_pct");
    if (val) t->cache_hit_pct = (uint8_t)cJSON_GetNumberValue(val);

    val = cJSON_GetObjectItem(data, "active_session_label");
    if (val && val->valuestring) strlcpy(t->active_session_label, val->valuestring, sizeof(t->active_session_label));
    val = cJSON_GetObjectItem(data, "active_session_age_s");
    if (val) t->active_session_age_s = (uint32_t)cJSON_GetNumberValue(val);

    cJSON *sessions = cJSON_GetObjectItem(data, "sessions");
    t->sess_ctx_count = 0;
    if (sessions && cJSON_IsArray(sessions)) {
        cJSON *s;
        cJSON_ArrayForEach(s, sessions) {
            if (t->sess_ctx_count >= CG_SESSIONS_MAX) break;
            session_context_t *sc = &t->sess_ctx[t->sess_ctx_count];
            val = cJSON_GetObjectItem(s, "label");
            if (val && val->valuestring) strlcpy(sc->label, val->valuestring, sizeof(sc->label));
            val = cJSON_GetObjectItem(s, "context_pct");
            if (val) sc->context_pct = (uint8_t)cJSON_GetNumberValue(val);
            t->sess_ctx_count++;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool oc_fetch_activity(app_state_t *state) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/activity", s_gw_url);
    if (!do_get(url, 3000)) return false;

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *arr = cJSON_GetObjectItem(root, "data");
    state->activity_count = 0;
    if (arr && cJSON_IsArray(arr)) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (state->activity_count >= 4) break;
            cJSON *v;
            v = cJSON_GetObjectItem(item, "ts");
            if (v && v->valuestring) strlcpy(state->activity[state->activity_count].ts, v->valuestring, 6);
            v = cJSON_GetObjectItem(item, "level");
            if (v && v->valuestring) strlcpy(state->activity[state->activity_count].level, v->valuestring, 6);
            v = cJSON_GetObjectItem(item, "msg");
            if (v && v->valuestring) strlcpy(state->activity[state->activity_count].msg, v->valuestring, 72);
            state->activity_count++;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool oc_fetch_transcript(app_state_t *state) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/transcript", s_gw_url);
    if (!do_get(url, 3000)) return false;

    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) return false;

    cJSON *arr = cJSON_GetObjectItem(root, "data");
    state->transcript_count = 0;
    if (arr && cJSON_IsArray(arr)) {
        // Count total entries
        int total = cJSON_GetArraySize(arr);
        // Skip to last 10
        int skip = total > 10 ? total - 10 : 0;
        int idx = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (idx++ < skip) continue;
            if (state->transcript_count >= 10) break;
            cJSON *v;
            v = cJSON_GetObjectItem(item, "ts");
            if (v && v->valuestring) strlcpy(state->transcript[state->transcript_count].ts, v->valuestring, 6);
            v = cJSON_GetObjectItem(item, "type");
            if (v && v->valuestring) strlcpy(state->transcript[state->transcript_count].type, v->valuestring, 6);
            v = cJSON_GetObjectItem(item, "text");
            if (v && v->valuestring) strlcpy(state->transcript[state->transcript_count].text, v->valuestring, 64);
            state->transcript_count++;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool oc_send_chat(const char *message, char *response, int response_len) {
    char url[192];
    snprintf(url, sizeof(url), "%s/api/clawglance/chat", s_gw_url);

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "model", "openclaw");
    cJSON_AddBoolToObject(body, "stream", false);
    cJSON_AddStringToObject(body, "user", "clawglance");

    cJSON *messages = cJSON_AddArrayToObject(body, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", message);
    cJSON_AddItemToArray(messages, msg);

    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    s_http_buf_len = 0;
    s_http_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 30000,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Authorization", s_auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-openclaw-agent-id", CG_OC_AGENT_ID);
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(json_str);
    s_http_buf[s_http_buf_len] = '\0';

    if (err != ESP_OK || status < 200 || status >= 300) {
        ESP_LOGW(TAG, "Chat error: status=%d", status);
        strlcpy(response, "[Request failed]", response_len);
        return false;
    }

    // Parse response
    cJSON *root = cJSON_Parse(s_http_buf);
    if (!root) {
        strlcpy(response, "[Parse error]", response_len);
        return false;
    }

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (choices && cJSON_GetArraySize(choices) > 0) {
        cJSON *first = cJSON_GetArrayItem(choices, 0);
        cJSON *msg_obj = cJSON_GetObjectItem(first, "message");
        cJSON *content = msg_obj ? cJSON_GetObjectItem(msg_obj, "content") : NULL;
        if (content && content->valuestring) {
            strlcpy(response, content->valuestring, response_len);
            cJSON_Delete(root);
            return true;
        }
    }

    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON *emsg = cJSON_GetObjectItem(error, "message");
        if (emsg && emsg->valuestring) {
            snprintf(response, response_len, "[Error] %s", emsg->valuestring);
        }
    } else {
        strlcpy(response, "[No content]", response_len);
    }

    cJSON_Delete(root);
    return false;
}
