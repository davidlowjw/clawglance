#include "ui_screens.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

// ---- Inter-screen communication ----
volatile int pending_command = -1;

// ---- Screen objects ----
static lv_obj_t *scr_dash = NULL;
static lv_obj_t *scr_telemetry = NULL;
static lv_obj_t *scr_send = NULL;
static lv_obj_t *scr_about = NULL;

// ---- OpenClaw color palette ----
#define OC_BG        0x080808
#define OC_ELEVATED  0x141416
#define OC_MUTED     0x1a1a1e
#define OC_BORDER    0x2a2a30
#define OC_RED       0xe5243b
#define OC_RED_LIGHT 0xff5c5c
#define OC_TEAL      0x0d9488
#define OC_AMBER     0xf59e0b
#define OC_GREEN     0x22c55e
#define OC_BLUE      0x3b82f6
#define OC_TEXT      0xfafafa
#define OC_TEXT_DIM  0x656d76
#define OC_TEXT_MUT  0x3a3a42

// ---- Layout ----
#define W 480
#define H 320
#define BAR_H 34
#define NAV_H 48
#define NAV_Y (H - NAV_H)

// ============================================================
// Dashboard widgets (Zone 1-6)
// ============================================================
// Z1: Status bar
static lv_obj_t *dash_time_lbl = NULL;
// Z2: Health strip
static lv_obj_t *dash_gw_lbl = NULL;
static lv_obj_t *dash_hb_lbl = NULL;
static lv_obj_t *dash_bridge_lbl = NULL;
static lv_obj_t *dash_model_lbl = NULL;
// Z3: Agent hero
static lv_obj_t *dash_status_lbl = NULL;
static lv_obj_t *dash_session_lbl = NULL;
static lv_obj_t *dash_duration_lbl = NULL;
static lv_obj_t *dash_hero_bar = NULL;
static lv_obj_t *dash_hero_ctx_lbl = NULL;
// Z4: Metric cards
static lv_obj_t *dash_cost_lbl = NULL;
static lv_obj_t *dash_cost_rate_lbl = NULL;
static lv_obj_t *dash_tokens_lbl = NULL;
static lv_obj_t *dash_sess_lbl = NULL;
static lv_obj_t *dash_sess_active_lbl = NULL;
// Z5: Budget + cache bars
static lv_obj_t *dash_bw_lbl = NULL;
static lv_obj_t *dash_bw_bar = NULL;
static lv_obj_t *dash_bw_pct = NULL;
static lv_obj_t *dash_bwk_lbl = NULL;
static lv_obj_t *dash_bwk_bar = NULL;
static lv_obj_t *dash_bwk_pct = NULL;
static lv_obj_t *dash_cache_bar = NULL;
static lv_obj_t *dash_cache_pct = NULL;

// Activity screen widgets are defined in build_activity_screen

// Send screen widgets
static lv_obj_t *send_response_lbl = NULL;

// ============================================================
// Shared helpers
// ============================================================
static void nav_btn_handler(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    ui_switch_to((screen_t)idx);
}

static void build_nav_bar(lv_obj_t *parent, int active_idx) {
    lv_obj_t *nav = lv_obj_create(parent);
    lv_obj_set_size(nav, W, NAV_H);
    lv_obj_set_pos(nav, 0, NAV_Y);
    lv_obj_set_style_bg_color(nav, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = { LV_SYMBOL_HOME " Dash", LV_SYMBOL_LIST " Activity", LV_SYMBOL_SETTINGS " Control" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(nav);
        lv_obj_set_size(btn, 148, 38);
        lv_obj_set_style_bg_color(btn, lv_color_hex(i == active_idx ? OC_RED : OC_MUTED), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, nav_btn_handler, LV_EVENT_CLICKED, NULL);
    }
}

static void build_status_bar(lv_obj_t *parent, lv_obj_t **time_lbl) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, W, BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Lobster emoji icon
    extern const lv_font_t lobster_icon;
    lv_obj_t *lobster = lv_label_create(bar);
    lv_label_set_text(lobster, "\xF0\x9F\xA6\x9E");  // U+1F99E lobster
    lv_obj_set_style_text_color(lobster, lv_color_hex(OC_RED), 0);
    lv_obj_set_style_text_font(lobster, &lobster_icon, 0);
    lv_obj_align(lobster, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "ClawGlance");
    lv_obj_set_style_text_color(title, lv_color_hex(OC_RED), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

    *time_lbl = lv_label_create(bar);
    lv_label_set_text(*time_lbl, "--:--");
    lv_obj_set_style_text_color(*time_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(*time_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(*time_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void format_tokens(char *buf, int len, uint32_t tok) {
    if (tok >= 1000000) snprintf(buf, len, "%.1fM", tok / 1000000.0f);
    else if (tok >= 1000) snprintf(buf, len, "%.1fk", tok / 1000.0f);
    else snprintf(buf, len, "%lu", (unsigned long)tok);
}

// Helper: make a bar row for telemetry page
                         lv_obj_t **out_bar, lv_obj_t **out_val) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl, 12, y + 2);

    *out_bar = lv_bar_create(parent);
    lv_obj_set_size(*out_bar, 260, 14);
    lv_obj_set_pos(*out_bar, 80, y + 2);
    lv_bar_set_range(*out_bar, 0, 100);
    lv_bar_set_value(*out_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(*out_bar, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(*out_bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(*out_bar, 4, 0);
    lv_obj_set_style_radius(*out_bar, 4, LV_PART_INDICATOR);

    *out_val = lv_label_create(parent);
    lv_label_set_text(*out_val, "0");
    lv_obj_set_style_text_color(*out_val, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(*out_val, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(*out_val, 350, y + 2);
}

// ============================================================
// DASHBOARD — Mission Control (7 zones)
// ============================================================
static void build_dash_screen(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // Z1: Status bar (y=0, h=28)
    build_status_bar(parent, &dash_time_lbl);

    // Z2: Health strip (y=28, h=22)
    lv_obj_t *health = lv_obj_create(parent);
    lv_obj_set_size(health, W, 22);
    lv_obj_set_pos(health, 0, BAR_H);
    lv_obj_set_style_bg_color(health, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_border_side(health, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(health, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(health, 1, 0);
    lv_obj_set_style_radius(health, 0, 0);
    lv_obj_set_style_pad_all(health, 0, 0);
    lv_obj_set_style_pad_left(health, 10, 0);
    lv_obj_set_flex_flow(health, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(health, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(health, 14, 0);
    lv_obj_clear_flag(health, LV_OBJ_FLAG_SCROLLABLE);

    dash_gw_lbl = lv_label_create(health);
    lv_label_set_text(dash_gw_lbl, LV_SYMBOL_WIFI " Gateway --");
    lv_obj_set_style_text_color(dash_gw_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_gw_lbl, &lv_font_montserrat_12, 0);

    dash_bridge_lbl = lv_label_create(health);
    lv_label_set_text(dash_bridge_lbl, LV_SYMBOL_LOOP " Bridge --");
    lv_obj_set_style_text_color(dash_bridge_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_bridge_lbl, &lv_font_montserrat_12, 0);

    dash_hb_lbl = lv_label_create(health);
    lv_label_set_text(dash_hb_lbl, LV_SYMBOL_OK " --");
    lv_obj_set_style_text_color(dash_hb_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_hb_lbl, &lv_font_montserrat_12, 0);

    int y = 50;

    // Z3: Agent status row (y=50, h=26)
    lv_obj_t *hero = lv_obj_create(parent);
    lv_obj_set_size(hero, W, 26);
    lv_obj_set_pos(hero, 0, y);
    lv_obj_set_style_bg_color(hero, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_border_width(hero, 0, 0);
    lv_obj_set_style_radius(hero, 0, 0);
    lv_obj_set_style_pad_all(hero, 0, 0);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);

    dash_status_lbl = lv_label_create(hero);
    lv_label_set_text(dash_status_lbl, LV_SYMBOL_PAUSE " IDLE");
    lv_obj_set_style_text_color(dash_status_lbl, lv_color_hex(OC_GREEN), 0);
    lv_obj_set_style_text_font(dash_status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(dash_status_lbl, 14, 4);

    dash_session_lbl = lv_label_create(hero);
    lv_label_set_text(dash_session_lbl, "");
    lv_obj_set_style_text_color(dash_session_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_session_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(dash_session_lbl, 160, 6);

    dash_duration_lbl = lv_label_create(hero);
    lv_label_set_text(dash_duration_lbl, "");
    lv_obj_set_style_text_color(dash_duration_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_duration_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(dash_duration_lbl, LV_ALIGN_TOP_RIGHT, -14, 6);
    y += 28;

    // Z3b: Context bar — distinct section with breathing room
    dash_hero_ctx_lbl = lv_label_create(parent);
    lv_label_set_text(dash_hero_ctx_lbl, "Context: --");
    lv_obj_set_style_text_color(dash_hero_ctx_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_hero_ctx_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_hero_ctx_lbl, 14, y);

    dash_hero_bar = lv_bar_create(parent);
    lv_obj_set_size(dash_hero_bar, W - 28, 10);
    lv_obj_set_pos(dash_hero_bar, 14, y + 16);
    lv_bar_set_range(dash_hero_bar, 0, 100);
    lv_bar_set_value(dash_hero_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(dash_hero_bar, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(dash_hero_bar, lv_color_hex(OC_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(dash_hero_bar, 4, 0);
    lv_obj_set_style_radius(dash_hero_bar, 4, LV_PART_INDICATOR);
    y += 32;

    // Z4: Three metric cards (y=100, h=80)
    int card_w = 148, card_h = 72, card_gap = 6;
    int card_x1 = 10, card_x2 = card_x1 + card_w + card_gap, card_x3 = card_x2 + card_w + card_gap;

    // Cost card
    lv_obj_t *c1 = lv_obj_create(parent);
    lv_obj_set_size(c1, card_w, card_h);
    lv_obj_set_pos(c1, card_x1, y);
    lv_obj_set_style_bg_color(c1, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_radius(c1, 8, 0);
    lv_obj_set_style_border_color(c1, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(c1, 1, 0);
    lv_obj_clear_flag(c1, LV_OBJ_FLAG_SCROLLABLE);

    dash_cost_lbl = lv_label_create(c1);
    lv_label_set_text(dash_cost_lbl, "$--.--");
    lv_obj_set_style_text_color(dash_cost_lbl, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(dash_cost_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(dash_cost_lbl, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *c1_sub = lv_label_create(c1);
    lv_label_set_text(c1_sub, "cost today");
    lv_obj_set_style_text_color(c1_sub, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(c1_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(c1_sub, LV_ALIGN_TOP_MID, 0, 28);

    dash_cost_rate_lbl = lv_label_create(c1);
    lv_label_set_text(dash_cost_rate_lbl, "");
    lv_obj_set_style_text_color(dash_cost_rate_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_cost_rate_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(dash_cost_rate_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Context card
    lv_obj_t *c2 = lv_obj_create(parent);
    lv_obj_set_size(c2, card_w, card_h);
    lv_obj_set_pos(c2, card_x2, y);
    lv_obj_set_style_bg_color(c2, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_radius(c2, 8, 0);
    lv_obj_set_style_border_color(c2, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(c2, 1, 0);
    lv_obj_clear_flag(c2, LV_OBJ_FLAG_SCROLLABLE);

    dash_tokens_lbl = lv_label_create(c2);
    lv_label_set_text(dash_tokens_lbl, "--");
    lv_obj_set_style_text_color(dash_tokens_lbl, lv_color_hex(OC_TEAL), 0);
    lv_obj_set_style_text_font(dash_tokens_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(dash_tokens_lbl, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *c2_sub = lv_label_create(c2);
    lv_label_set_text(c2_sub, "tokens today");
    lv_obj_set_style_text_color(c2_sub, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(c2_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(c2_sub, LV_ALIGN_TOP_MID, 0, 28);

    // Sessions card
    lv_obj_t *c3 = lv_obj_create(parent);
    lv_obj_set_size(c3, card_w, card_h);
    lv_obj_set_pos(c3, card_x3, y);
    lv_obj_set_style_bg_color(c3, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_radius(c3, 8, 0);
    lv_obj_set_style_border_color(c3, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(c3, 1, 0);
    lv_obj_clear_flag(c3, LV_OBJ_FLAG_SCROLLABLE);

    dash_sess_lbl = lv_label_create(c3);
    lv_label_set_text(dash_sess_lbl, "0 active");
    lv_obj_set_style_text_color(dash_sess_lbl, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(dash_sess_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(dash_sess_lbl, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *c3_sub = lv_label_create(c3);
    lv_label_set_text(c3_sub, "active sessions");
    lv_obj_set_style_text_color(c3_sub, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(c3_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(c3_sub, LV_ALIGN_TOP_MID, 0, 28);

    dash_sess_active_lbl = lv_label_create(c3);
    lv_label_set_text(dash_sess_active_lbl, "");
    lv_obj_set_style_text_color(dash_sess_active_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_sess_active_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(dash_sess_active_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    y += card_h + 4;

    // Z5: Model + Budget bars + cache
    // Model line
    dash_model_lbl = lv_label_create(parent);
    lv_label_set_text(dash_model_lbl, "");
    lv_obj_set_style_text_color(dash_model_lbl, lv_color_hex(OC_TEAL), 0);
    lv_obj_set_style_text_font(dash_model_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_model_lbl, 10, y + 2);
    y += 16;

    // Row 1: window bar
    dash_bw_lbl = lv_label_create(parent);
    lv_label_set_text(dash_bw_lbl, "5h window");
    lv_obj_set_style_text_color(dash_bw_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_bw_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_bw_lbl, 10, y + 2);

    dash_bw_bar = lv_bar_create(parent);
    lv_obj_set_size(dash_bw_bar, 180, 10);
    lv_obj_set_pos(dash_bw_bar, 85, y + 4);
    lv_bar_set_range(dash_bw_bar, 0, 100);
    lv_obj_set_style_bg_color(dash_bw_bar, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(dash_bw_bar, lv_color_hex(OC_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(dash_bw_bar, 3, 0);
    lv_obj_set_style_radius(dash_bw_bar, 3, LV_PART_INDICATOR);

    dash_bw_pct = lv_label_create(parent);
    lv_label_set_text(dash_bw_pct, "--%");
    lv_obj_set_style_text_color(dash_bw_pct, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(dash_bw_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_bw_pct, 272, y + 2);
    y += 16;

    // Row 2: week bar
    dash_bwk_lbl = lv_label_create(parent);
    lv_label_set_text(dash_bwk_lbl, "Week");
    lv_obj_set_style_text_color(dash_bwk_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(dash_bwk_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_bwk_lbl, 10, y + 2);

    dash_bwk_bar = lv_bar_create(parent);
    lv_obj_set_size(dash_bwk_bar, 180, 10);
    lv_obj_set_pos(dash_bwk_bar, 85, y + 4);
    lv_bar_set_range(dash_bwk_bar, 0, 100);
    lv_obj_set_style_bg_color(dash_bwk_bar, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(dash_bwk_bar, lv_color_hex(OC_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(dash_bwk_bar, 3, 0);
    lv_obj_set_style_radius(dash_bwk_bar, 3, LV_PART_INDICATOR);

    dash_bwk_pct = lv_label_create(parent);
    lv_label_set_text(dash_bwk_pct, "--%");
    lv_obj_set_style_text_color(dash_bwk_pct, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(dash_bwk_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_bwk_pct, 272, y + 2);
    y += 16;

    // Row 3: cache hit bar
    lv_obj_t *ch_lbl = lv_label_create(parent);
    lv_label_set_text(ch_lbl, "Cache hit");
    lv_obj_set_style_text_color(ch_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(ch_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ch_lbl, 10, y + 2);

    dash_cache_bar = lv_bar_create(parent);
    lv_obj_set_size(dash_cache_bar, 180, 10);
    lv_obj_set_pos(dash_cache_bar, 85, y + 4);
    lv_bar_set_range(dash_cache_bar, 0, 100);
    lv_obj_set_style_bg_color(dash_cache_bar, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(dash_cache_bar, lv_color_hex(OC_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_radius(dash_cache_bar, 3, 0);
    lv_obj_set_style_radius(dash_cache_bar, 3, LV_PART_INDICATOR);

    dash_cache_pct = lv_label_create(parent);
    lv_label_set_text(dash_cache_pct, "--%");
    lv_obj_set_style_text_color(dash_cache_pct, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(dash_cache_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dash_cache_pct, 272, y + 2);
    y += 18;

    build_nav_bar(parent, 0);
}

// ============================================================
// ACTIVITY Screen — scrollable session transcript feed
// ============================================================
static lv_obj_t *act_time_lbl = NULL;
static lv_obj_t *act_list = NULL;

static void build_activity_screen(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    build_status_bar(parent, &act_time_lbl);

    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, LV_SYMBOL_LIST " Session Activity");
    lv_obj_set_style_text_color(hdr, lv_color_hex(OC_TEAL), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(hdr, 12, BAR_H + 4);

    // Scrollable container for transcript rows
    int y = BAR_H + 24;
    act_list = lv_obj_create(parent);
    lv_obj_set_size(act_list, W, NAV_Y - y);
    lv_obj_set_pos(act_list, 0, y);
    lv_obj_set_style_bg_color(act_list, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_border_width(act_list, 0, 0);
    lv_obj_set_style_radius(act_list, 0, 0);
    lv_obj_set_style_pad_all(act_list, 4, 0);
    lv_obj_set_style_pad_row(act_list, 6, 0);
    lv_obj_set_flex_flow(act_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(act_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(act_list, LV_SCROLLBAR_MODE_ACTIVE);

    build_nav_bar(parent, 1);
}

static char act_last_hash[64] = "";

void activity_update(const app_state_t *state) {
    if (!act_list) return;

    // Build a simple hash from count + last entry text to detect changes
    char hash[64] = "";
    if (state->transcript_count > 0) {
        snprintf(hash, sizeof(hash), "%d:%s",
            state->transcript_count,
            state->transcript[state->transcript_count - 1].text);
    }

    if (strcmp(hash, act_last_hash) == 0) {
        if (act_time_lbl && state->time_string[0])
            lv_label_set_text(act_time_lbl, state->time_string);
        return;
    }
    strlcpy(act_last_hash, hash, sizeof(act_last_hash));

    lv_obj_clean(act_list);

    for (int i = 0; i < state->transcript_count; i++) {
        uint32_t color;
        const char *prefix;
        if (strcmp(state->transcript[i].type, "user") == 0) {
            color = OC_GREEN;
            prefix = ">";
        } else if (strcmp(state->transcript[i].type, "tool") == 0) {
            color = OC_AMBER;
            prefix = LV_SYMBOL_SETTINGS;
        } else {
            color = OC_TEXT_DIM;
            prefix = "<";
        }

        char buf[80];
        snprintf(buf, sizeof(buf), "%s %s  %s", state->transcript[i].ts, prefix, state->transcript[i].text);

        lv_obj_t *row = lv_label_create(act_list);
        lv_label_set_text(row, buf);
        lv_label_set_long_mode(row, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(row, W - 20);
        lv_obj_set_style_text_color(row, lv_color_hex(color), 0);
        lv_obj_set_style_text_font(row, &lv_font_montserrat_12, 0);
    }

    // Scroll to bottom to show latest
    lv_obj_scroll_to_y(act_list, LV_COORD_MAX, LV_ANIM_OFF);

    if (act_time_lbl && state->time_string[0]) {
        lv_label_set_text(act_time_lbl, state->time_string);
    }
}

// ============================================================
// CONTROL Screen
// ============================================================
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

static bool backlight_pwm_inited = false;
static uint8_t current_brightness = 255;

static void init_backlight_pwm(void) {
    if (backlight_pwm_inited) return;
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t channel = {
        .gpio_num = 1,  // GPIO_LCD_BL
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = current_brightness,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
    backlight_pwm_inited = true;
}

static void set_brightness(uint8_t val) {
    current_brightness = val;
    init_backlight_pwm();
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, val);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void save_brightness(uint8_t val) {
    nvs_handle_t h;
    if (nvs_open("clawglance", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "brightness", val);
        nvs_commit(h);
        nvs_close(h);
    }
}

static uint8_t load_brightness(void) {
    nvs_handle_t h;
    uint8_t val = 255;
    if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "brightness", &val);
        nvs_close(h);
    }
    return val < 10 ? 10 : val;
}

static void brightness_slider_handler(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    set_brightness((uint8_t)val);
    save_brightness((uint8_t)val);
}

// Text area focus/defocus — show/hide keyboard
static void cfg_ta_focus_handler(lv_event_t *e) {
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void cfg_ta_defocus_handler(lv_event_t *e) {
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

// Save config to NVS and reboot
static lv_obj_t *s_wifi_ta, *s_pass_ta, *s_gw_ta;

static void save_config_handler(lv_event_t *e) {
    (void)e;
    nvs_handle_t h;
    if (nvs_open("clawglance", NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_str(h, "wifi_ssid", lv_textarea_get_text(s_wifi_ta));
    nvs_set_str(h, "wifi_pass", lv_textarea_get_text(s_pass_ta));
    nvs_set_str(h, "gw_host", lv_textarea_get_text(s_gw_ta));
    nvs_commit(h);
    nvs_close(h);

    // Reboot
    esp_restart();
}

static void cmd_btn_handler(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    int cmd = (int)(intptr_t)lv_obj_get_user_data(btn);
    pending_command = cmd;
    if (send_response_lbl) lv_label_set_text(send_response_lbl, "Sending...");
}

static void build_send_screen(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, W, BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Controls");
    lv_obj_set_style_text_color(title, lv_color_hex(OC_TEAL), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    // Action buttons — same row
    int btn_w = (W - 50) / 2;
    lv_obj_t *status_btn = lv_btn_create(parent);
    lv_obj_set_size(status_btn, btn_w, 34);
    lv_obj_set_pos(status_btn, 15, BAR_H + 8);
    lv_obj_set_style_bg_color(status_btn, lv_color_hex(OC_BLUE), 0);
    lv_obj_set_style_radius(status_btn, 8, 0);
    lv_obj_t *s_lbl = lv_label_create(status_btn);
    lv_label_set_text(s_lbl, LV_SYMBOL_DOWNLOAD " Refresh");
    lv_obj_set_style_text_font(s_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(s_lbl);
    lv_obj_set_user_data(status_btn, (void *)(intptr_t)1);
    lv_obj_add_event_cb(status_btn, cmd_btn_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_w, 34);
    lv_obj_set_pos(btn, W / 2 + 5, BAR_H + 8);
    lv_obj_set_style_bg_color(btn, lv_color_hex(OC_RED), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH " Restart GW");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
    lv_obj_set_user_data(btn, (void *)(intptr_t)0);
    lv_obj_add_event_cb(btn, cmd_btn_handler, LV_EVENT_CLICKED, NULL);

    // Brightness slider
    int slider_y = BAR_H + 48;
    lv_obj_t *bright_lbl = lv_label_create(parent);
    lv_label_set_text(bright_lbl, LV_SYMBOL_IMAGE " Brightness");
    lv_obj_set_style_text_color(bright_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(bright_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(bright_lbl, 20, slider_y + 4);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_size(slider, W - 160, 14);
    lv_obj_set_pos(slider, 130, slider_y + 6);
    lv_slider_set_range(slider, 10, 255);
    current_brightness = load_brightness();
    lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
    set_brightness(current_brightness);
    lv_obj_set_style_bg_color(slider, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_bg_color(slider, lv_color_hex(OC_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(OC_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, brightness_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Configuration section ----
    int cfg_y = slider_y + 30;

    // Separator
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, W - 40, 1);
    lv_obj_set_pos(sep, 20, cfg_y);
    lv_obj_set_style_bg_color(sep, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    cfg_y += 8;

    // WiFi SSID
    lv_obj_t *wifi_lbl = lv_label_create(parent);
    lv_label_set_text(wifi_lbl, "WiFi SSID");
    lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(wifi_lbl, 20, cfg_y);

    s_wifi_ta = lv_textarea_create(parent);
    lv_obj_set_size(s_wifi_ta, W - 100, 30);
    lv_obj_set_pos(s_wifi_ta, 90, cfg_y - 4);
    lv_textarea_set_one_line(s_wifi_ta, true);
    lv_textarea_set_placeholder_text(s_wifi_ta, "SSID");
    lv_obj_set_style_bg_color(s_wifi_ta, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_text_color(s_wifi_ta, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(s_wifi_ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(s_wifi_ta, lv_color_hex(OC_BORDER), 0);
    // Load saved SSID
    {
        nvs_handle_t h;
        char buf[33] = "";
        size_t len = sizeof(buf);
        if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
            nvs_get_str(h, "wifi_ssid", buf, &len);
            nvs_close(h);
        }
        if (buf[0] == '\0') strlcpy(buf, CG_WIFI_SSID, sizeof(buf));
        lv_textarea_set_text(s_wifi_ta, buf);
    }
    cfg_y += 32;

    // WiFi Password
    lv_obj_t *pass_lbl = lv_label_create(parent);
    lv_label_set_text(pass_lbl, "Password");
    lv_obj_set_style_text_color(pass_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(pass_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(pass_lbl, 20, cfg_y);

    
    s_pass_ta = lv_textarea_create(parent);
    lv_obj_set_size(s_pass_ta, W - 100, 30);
    lv_obj_set_pos(s_pass_ta, 90, cfg_y - 4);
    lv_textarea_set_one_line(s_pass_ta, true);
    lv_textarea_set_placeholder_text(s_pass_ta, "Password");
    lv_textarea_set_password_mode(s_pass_ta, true);
    lv_obj_set_style_bg_color(s_pass_ta, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_text_color(s_pass_ta, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(s_pass_ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(s_pass_ta, lv_color_hex(OC_BORDER), 0);
    {
        nvs_handle_t h;
        char buf[65] = "";
        size_t len = sizeof(buf);
        if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
            nvs_get_str(h, "wifi_pass", buf, &len);
            nvs_close(h);
        }
        if (buf[0] == '\0') strlcpy(buf, CG_WIFI_PASS, sizeof(buf));
        lv_textarea_set_text(s_pass_ta, buf);
    }
    cfg_y += 32;

    // Gateway Host
    lv_obj_t *gw_lbl = lv_label_create(parent);
    lv_label_set_text(gw_lbl, "Gateway");
    lv_obj_set_style_text_color(gw_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(gw_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(gw_lbl, 20, cfg_y);

    
    s_gw_ta = lv_textarea_create(parent);
    lv_obj_set_size(s_gw_ta, W - 100, 30);
    lv_obj_set_pos(s_gw_ta, 90, cfg_y - 4);
    lv_textarea_set_one_line(s_gw_ta, true);
    lv_textarea_set_placeholder_text(s_gw_ta, "192.168.1.100:18789");
    lv_obj_set_style_bg_color(s_gw_ta, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_text_color(s_gw_ta, lv_color_hex(OC_TEXT), 0);
    lv_obj_set_style_text_font(s_gw_ta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(s_gw_ta, lv_color_hex(OC_BORDER), 0);
    {
        nvs_handle_t h;
        char buf[48] = "";
        size_t len = sizeof(buf);
        if (nvs_open("clawglance", NVS_READONLY, &h) == ESP_OK) {
            nvs_get_str(h, "gw_host", buf, &len);
            nvs_close(h);
        }
        if (buf[0] == '\0') snprintf(buf, sizeof(buf), "%s:%d", CG_OC_HOST, CG_OC_PORT);
        lv_textarea_set_text(s_gw_ta, buf);
    }
    cfg_y += 32;

    cfg_y += 4;

    // On-screen keyboard
    static lv_obj_t *cfg_kb = NULL;
    cfg_kb = lv_keyboard_create(parent);
    lv_obj_set_size(cfg_kb, W, 140);
    lv_obj_align(cfg_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(cfg_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(cfg_kb, lv_color_hex(OC_ELEVATED), 0);
    lv_obj_set_style_text_color(cfg_kb, lv_color_hex(OC_TEXT), 0);

    // Bind keyboard to text areas — LVGL handles show/hide automatically
    lv_obj_t *all_ta[] = { s_wifi_ta, s_pass_ta, s_gw_ta };
    for (int i = 0; i < 3; i++) {
        lv_obj_add_event_cb(all_ta[i], cfg_ta_focus_handler, LV_EVENT_FOCUSED, cfg_kb);
        lv_obj_add_event_cb(all_ta[i], cfg_ta_defocus_handler, LV_EVENT_DEFOCUSED, cfg_kb);
    }

    // Save & Reboot button
    lv_obj_t *save_btn = lv_btn_create(parent);
    lv_obj_set_size(save_btn, W - 40, 38);
    lv_obj_set_pos(save_btn, 20, cfg_y);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(OC_GREEN), 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " Save & Reboot");
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, save_config_handler, LV_EVENT_CLICKED, NULL);

    // Response area (smaller)
    send_response_lbl = lv_label_create(parent);
    lv_label_set_text(send_response_lbl, "");
    lv_obj_set_style_text_color(send_response_lbl, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(send_response_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(send_response_lbl, 20, cfg_y + 42);

    build_nav_bar(parent, 2);
}

// ============================================================
// Public API
// ============================================================
// ============================================================
// ============================================================
// Swipe navigation: About <-> Dash <-> Activity <-> Control
// ============================================================
// Screen order: [0]=about, [1]=dash, [2]=activity, [3]=control
static lv_obj_t **scr_order[4];
static int scr_order_count = 4;

static int find_screen_index(lv_obj_t *scr) {
    for (int i = 0; i < scr_order_count; i++) {
        if (*scr_order[i] == scr) return i;
    }
    return 1; // default to dash
}

static void swipe_handler(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    lv_obj_t *current = lv_event_get_current_target(e);
    int idx = find_screen_index(current);

    if (dir == LV_DIR_LEFT && idx < scr_order_count - 1) {
        lv_scr_load_anim(*scr_order[idx + 1], LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
    } else if (dir == LV_DIR_RIGHT && idx > 0) {
        lv_scr_load_anim(*scr_order[idx - 1], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
    }
}

static void build_about_screen(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(OC_BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    int y = 30;

    // Large lobster emoji
    extern const lv_font_t lobster_icon_lg;
    lv_obj_t *logo = lv_label_create(parent);
    lv_label_set_text(logo, "\xF0\x9F\xA6\x9E");
    lv_obj_set_style_text_color(logo, lv_color_hex(OC_RED), 0);
    lv_obj_set_style_text_font(logo, &lobster_icon_lg, 0);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, y);
    y += 56;

    // "ClawGlance" title
    lv_obj_t *cg = lv_label_create(parent);
    lv_label_set_text(cg, "ClawGlance");
    lv_obj_set_style_text_color(cg, lv_color_hex(OC_RED), 0);
    lv_obj_set_style_text_font(cg, &lv_font_montserrat_16, 0);
    lv_obj_align(cg, LV_ALIGN_TOP_MID, 0, y);
    y += 22;

    // ClawGlance version
    lv_obj_t *cgv = lv_label_create(parent);
    lv_label_set_text(cgv, "v" CLAWGLANCE_VERSION);
    lv_obj_set_style_text_color(cgv, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(cgv, &lv_font_montserrat_12, 0);
    lv_obj_align(cgv, LV_ALIGN_TOP_MID, 0, y);
    y += 24;

    // Separator
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 200, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(OC_BORDER), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, y);
    y += 12;

    // OpenClaw version
    lv_obj_t *oc = lv_label_create(parent);
    lv_label_set_text(oc, "OpenClaw v2026.3.24");
    lv_obj_set_style_text_color(oc, lv_color_hex(OC_TEAL), 0);
    lv_obj_set_style_text_font(oc, &lv_font_montserrat_14, 0);
    lv_obj_align(oc, LV_ALIGN_TOP_MID, 0, y);
    y += 24;

    // System info
    lv_obj_t *info = lv_label_create(parent);
    lv_label_set_text(info, "ESP32-S3 | 480x320 | 8MB PSRAM");
    lv_obj_set_style_text_color(info, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    // Gateway info
    lv_obj_t *gw = lv_label_create(parent);
    char gw_buf[48];
    snprintf(gw_buf, sizeof(gw_buf), "Gateway: %s:%d", CG_OC_HOST, CG_OC_PORT);
    lv_label_set_text(gw, gw_buf);
    lv_obj_set_style_text_color(gw, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(gw, &lv_font_montserrat_12, 0);
    lv_obj_align(gw, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    // Bridge info
    lv_obj_t *br = lv_label_create(parent);
    char br_buf[48];
    snprintf(br_buf, sizeof(br_buf), "Bridge: %s:%d", CG_OC_HOST, CG_OC_DASH_PORT);
    lv_label_set_text(br, br_buf);
    lv_obj_set_style_text_color(br, lv_color_hex(OC_TEXT_DIM), 0);
    lv_obj_set_style_text_font(br, &lv_font_montserrat_12, 0);
    lv_obj_align(br, LV_ALIGN_TOP_MID, 0, y);
    y += 28;

    // Credits
    lv_obj_t *credits = lv_label_create(parent);
    lv_label_set_text(credits, "Built with Claude Code");
    lv_obj_set_style_text_color(credits, lv_color_hex(OC_TEXT_MUT), 0);
    lv_obj_set_style_text_font(credits, &lv_font_montserrat_12, 0);
    lv_obj_align(credits, LV_ALIGN_TOP_MID, 0, y);

    // Hint
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "Swipe left to go back");
    lv_obj_set_style_text_color(hint, lv_color_hex(OC_TEXT_MUT), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);

    // Swipe gesture
    lv_obj_add_event_cb(parent, swipe_handler, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void ui_build_all(void) {
    scr_dash = lv_obj_create(NULL);
    scr_telemetry = lv_obj_create(NULL);
    scr_send = lv_obj_create(NULL);
    scr_about = lv_obj_create(NULL);

    build_dash_screen(scr_dash);
    build_activity_screen(scr_telemetry);
    build_send_screen(scr_send);
    build_about_screen(scr_about);

    // Set up swipe order: About <-> Dash <-> Activity <-> Control
    scr_order[0] = &scr_about;
    scr_order[1] = &scr_dash;
    scr_order[2] = &scr_telemetry;  // activity screen
    scr_order[3] = &scr_send;       // control screen

    // Add swipe gesture to all screens
    lv_obj_t *screens[] = { scr_dash, scr_telemetry, scr_send };
    for (int i = 0; i < 3; i++) {
        lv_obj_add_event_cb(screens[i], swipe_handler, LV_EVENT_GESTURE, NULL);
        lv_obj_clear_flag(screens[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
    // About already has swipe handler from build_about_screen

    lv_scr_load(scr_dash);
}

void ui_switch_to(screen_t screen) {
    lv_obj_t *target = NULL;
    switch (screen) {
        case SCREEN_DASH:      target = scr_dash; break;
        case SCREEN_ACTIVITY: target = scr_telemetry; break;
        case SCREEN_SEND:      target = scr_send; break;
    }
    if (target) lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}


// ---- Dashboard update functions ----

void dash_update_time(const char *time_str) {
    if (dash_time_lbl) lv_label_set_text(dash_time_lbl, time_str);
    if (act_time_lbl) lv_label_set_text(act_time_lbl, time_str);
}

void dash_update_health(bool gw_online, bool bridge_online, uint32_t last_fetch_age_s, const char *model) {
    if (dash_gw_lbl) {
        lv_label_set_text(dash_gw_lbl, gw_online ? LV_SYMBOL_WIFI " Gateway OK" : LV_SYMBOL_WIFI " Gateway --");
        lv_obj_set_style_text_color(dash_gw_lbl, lv_color_hex(gw_online ? OC_GREEN : OC_RED), 0);
    }
    if (dash_bridge_lbl) {
        lv_label_set_text(dash_bridge_lbl, bridge_online ? LV_SYMBOL_LOOP " Bridge OK" : LV_SYMBOL_LOOP " Bridge --");
        lv_obj_set_style_text_color(dash_bridge_lbl, lv_color_hex(bridge_online ? OC_GREEN : OC_RED), 0);
    }
    if (dash_hb_lbl) {
        char buf[20];
        if (last_fetch_age_s < 60) snprintf(buf, sizeof(buf), LV_SYMBOL_OK " %lus", (unsigned long)last_fetch_age_s);
        else snprintf(buf, sizeof(buf), LV_SYMBOL_OK " %lum", (unsigned long)(last_fetch_age_s / 60));
        lv_label_set_text(dash_hb_lbl, buf);
        uint32_t c = last_fetch_age_s < 30 ? OC_GREEN : (last_fetch_age_s < 120 ? OC_AMBER : OC_RED);
        lv_obj_set_style_text_color(dash_hb_lbl, lv_color_hex(c), 0);
    }
    if (dash_model_lbl && model && model[0]) {
        lv_label_set_text(dash_model_lbl, model);
    }
}

void dash_update_activity(agent_status_t status, const char *session_label, uint32_t duration_s, uint8_t context_pct, uint32_t context_used, uint32_t context_max) {
    if (dash_status_lbl) {
        const char *icon = status == AGENT_ACTIVE ? LV_SYMBOL_PLAY : (status == AGENT_ERROR ? LV_SYMBOL_WARNING : LV_SYMBOL_PAUSE);
        const char *text = status == AGENT_ACTIVE ? "ACTIVE" : (status == AGENT_ERROR ? "ERROR" : "IDLE");
        uint32_t color = status == AGENT_ACTIVE ? OC_BLUE : (status == AGENT_ERROR ? OC_RED : OC_GREEN);
        char buf[24];
        snprintf(buf, sizeof(buf), "%s %s", icon, text);
        lv_label_set_text(dash_status_lbl, buf);
        lv_obj_set_style_text_color(dash_status_lbl, lv_color_hex(color), 0);
        if (dash_hero_bar) lv_obj_set_style_bg_color(dash_hero_bar, lv_color_hex(color), LV_PART_INDICATOR);
    }
    if (dash_session_lbl) lv_label_set_text(dash_session_lbl, session_label ? session_label : "");
    if (dash_duration_lbl) {
        if (duration_s > 0 && status == AGENT_ACTIVE) {
            char buf[16];
            if (duration_s >= 3600) snprintf(buf, sizeof(buf), "%luh %lum", (unsigned long)(duration_s / 3600), (unsigned long)((duration_s % 3600) / 60));
            else if (duration_s >= 60) snprintf(buf, sizeof(buf), "%lum %lus", (unsigned long)(duration_s / 60), (unsigned long)(duration_s % 60));
            else snprintf(buf, sizeof(buf), "%lus", (unsigned long)duration_s);
            lv_label_set_text(dash_duration_lbl, buf);
        } else {
            lv_label_set_text(dash_duration_lbl, "");
        }
    }
    if (dash_hero_bar) {
        lv_bar_set_value(dash_hero_bar, context_pct > 100 ? 100 : context_pct, LV_ANIM_ON);
        uint32_t c = context_pct < 60 ? OC_TEAL : (context_pct < 80 ? OC_AMBER : OC_RED);
        lv_obj_set_style_bg_color(dash_hero_bar, lv_color_hex(c), LV_PART_INDICATOR);
    }
    if (dash_hero_ctx_lbl && context_max > 0) {
        char used_buf[12], max_buf[12];
        format_tokens(used_buf, sizeof(used_buf), context_used);
        format_tokens(max_buf, sizeof(max_buf), context_max);
        char buf[48];
        snprintf(buf, sizeof(buf), "Context: %d%% (%s / %s)", context_pct, used_buf, max_buf);
        lv_label_set_text(dash_hero_ctx_lbl, buf);
        uint32_t c = context_pct < 60 ? OC_TEXT_DIM : (context_pct < 80 ? OC_AMBER : OC_RED);
        lv_obj_set_style_text_color(dash_hero_ctx_lbl, lv_color_hex(c), 0);
    }
}

void dash_update_cost(float cost_today, float rate_per_hour) {
    if (dash_cost_lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "$%.2f", cost_today);
        lv_label_set_text(dash_cost_lbl, buf);
        uint32_t c = cost_today < 2 ? OC_GREEN : (cost_today < 5 ? OC_TEXT : (cost_today < 8 ? OC_AMBER : OC_RED));
        lv_obj_set_style_text_color(dash_cost_lbl, lv_color_hex(c), 0);
    }
    if (dash_cost_rate_lbl) {
        if (rate_per_hour > 0.001f) {
            char buf[20];
            snprintf(buf, sizeof(buf), "$%.2f/hr", rate_per_hour);
            lv_label_set_text(dash_cost_rate_lbl, buf);
        } else {
            lv_label_set_text(dash_cost_rate_lbl, "");
        }
    }
}

void dash_update_tokens(uint32_t tokens_today) {
    if (dash_tokens_lbl) {
        char buf[16];
        format_tokens(buf, sizeof(buf), tokens_today);
        lv_label_set_text(dash_tokens_lbl, buf);
    }
}

void dash_update_sessions(uint8_t active, uint8_t total) {
    (void)total;
    if (dash_sess_lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", active);
        lv_label_set_text(dash_sess_lbl, buf);
    }
}

void dash_update_info(uint8_t budget_window_pct, const char *window_label, uint8_t budget_week_pct, uint8_t cache_hit_pct) {
    // Window budget bar — show USED (100 - remaining)
    uint8_t win_used = budget_window_pct > 100 ? 0 : 100 - budget_window_pct;
    if (dash_bw_lbl && window_label && window_label[0]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s window", window_label);
        lv_label_set_text(dash_bw_lbl, buf);
    }
    if (dash_bw_bar) {
        lv_bar_set_value(dash_bw_bar, win_used, LV_ANIM_ON);
        uint32_t c = win_used < 50 ? OC_GREEN : (win_used < 75 ? OC_AMBER : OC_RED);
        lv_obj_set_style_bg_color(dash_bw_bar, lv_color_hex(c), LV_PART_INDICATOR);
    }
    if (dash_bw_pct) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d%% used", win_used);
        lv_label_set_text(dash_bw_pct, buf);
        uint32_t c = win_used < 50 ? OC_GREEN : (win_used < 75 ? OC_AMBER : OC_RED);
        lv_obj_set_style_text_color(dash_bw_pct, lv_color_hex(c), 0);
    }

    // Week budget bar — show USED
    uint8_t week_used = budget_week_pct > 100 ? 0 : 100 - budget_week_pct;
    if (dash_bwk_bar) {
        lv_bar_set_value(dash_bwk_bar, week_used, LV_ANIM_ON);
        uint32_t c = week_used < 50 ? OC_GREEN : (week_used < 75 ? OC_AMBER : OC_RED);
        lv_obj_set_style_bg_color(dash_bwk_bar, lv_color_hex(c), LV_PART_INDICATOR);
    }
    if (dash_bwk_pct) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d%% used", week_used);
        lv_label_set_text(dash_bwk_pct, buf);
        uint32_t c = week_used < 50 ? OC_GREEN : (week_used < 75 ? OC_AMBER : OC_RED);
        lv_obj_set_style_text_color(dash_bwk_pct, lv_color_hex(c), 0);
    }

    // Cache hit bar
    if (dash_cache_bar) {
        lv_bar_set_value(dash_cache_bar, cache_hit_pct, LV_ANIM_ON);
        uint32_t c = cache_hit_pct > 50 ? OC_TEAL : (cache_hit_pct > 20 ? OC_AMBER : OC_RED);
        lv_obj_set_style_bg_color(dash_cache_bar, lv_color_hex(c), LV_PART_INDICATOR);
    }
    if (dash_cache_pct) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d%%", cache_hit_pct);
        lv_label_set_text(dash_cache_pct, buf);
        uint32_t c = cache_hit_pct > 50 ? OC_TEAL : (cache_hit_pct > 20 ? OC_AMBER : OC_RED);
        lv_obj_set_style_text_color(dash_cache_pct, lv_color_hex(c), 0);
    }
}




// Legacy stubs

void send_update_response(const char *text, bool loading) {
    (void)loading;
    if (send_response_lbl) lv_label_set_text(send_response_lbl, text);
}
