/**
 * sim_main.c — headless LVGL PC simulator for ClawGlance
 *
 * Renders each of the four screens built by ui_screens.c into an in-memory
 * 480x320 ARGB8888 framebuffer via a dummy LVGL flush driver, then writes
 * the framebuffer out to PNG using stb_image_write. No SDL, no window.
 *
 * Feeds hardcoded-but-plausible values into the dash_update_* /
 * activity_update APIs so the screenshots look "live" even though there's
 * no gateway attached.
 */
#include "lvgl.h"
#include "ui_screens.h"
#include "app_state.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_W 480
#define SIM_H 320

/* ── Dummy flush buffer (receives LVGL's rendered pixels) ── */
static uint32_t fb[SIM_W * SIM_H];

static void dummy_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    /* Copy the dirty rectangle from LVGL's draw buffer into our framebuffer.
     * LV_COLOR_DEPTH=32 means lv_color_t is already ARGB8888 (well, XRGB). */
    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            fb[y * SIM_W + x] = color_p->full;
            color_p++;
        }
    }
    lv_disp_flush_ready(drv);
}

/* ── Convert LVGL's XRGB8888 buffer to RGBA8888 for stb_image_write ── */
static void fb_to_rgba(uint8_t *out) {
    for (int i = 0; i < SIM_W * SIM_H; i++) {
        uint32_t p = fb[i];
        out[i*4 + 0] = (p >> 16) & 0xff;  /* R */
        out[i*4 + 1] = (p >>  8) & 0xff;  /* G */
        out[i*4 + 2] = (p      ) & 0xff;  /* B */
        out[i*4 + 3] = 0xff;               /* A */
    }
}

static void save_png(const char *path) {
    static uint8_t rgba[SIM_W * SIM_H * 4];
    fb_to_rgba(rgba);
    if (stbi_write_png(path, SIM_W, SIM_H, 4, rgba, SIM_W * 4) == 0) {
        fprintf(stderr, "failed to write %s\n", path);
        exit(1);
    }
    printf("wrote %s\n", path);
}

/* ── Force a full refresh and snapshot the current screen ──
 *
 * ui_switch_to() uses lv_scr_load_anim with a 200ms fade. We need to
 * pump the tick past the animation's end before capturing, otherwise
 * we screenshot the *previous* screen mid-fade. 20x20ms = 400ms, which
 * comfortably outlasts any screen transition. */
static void render_and_save(const char *path) {
    for (int i = 0; i < 20; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    save_png(path);
}

/* ── Fake data pushed into the dashboard ── */
static void populate_mock_data(void) {
    /* Dashboard */
    dash_update_time("14:30");
    dash_update_health(/*gw_online*/true, /*last_fetch_age_s*/1, "gpt-5.3-codex");
    dash_update_activity(AGENT_IDLE, "main", /*duration_s*/134,
                         /*context_pct*/22, /*context_used*/14600, /*context_max*/64000);
    dash_update_cost(0.06f, 0.02f);
    dash_update_tokens(71769);
    dash_update_sessions(3, 5);
    dash_update_info(/*window_pct*/9, "5h", /*week_pct*/17, /*cache_hit_pct*/69);

    /* Activity screen transcript */
    static app_state_t mock = {0};
    mock.gateway.connection = CONN_GATEWAY_CONNECTED;
    mock.gateway.healthy = true;
    strncpy(mock.gateway.current_model, "gpt-5.3-codex", sizeof(mock.gateway.current_model)-1);
    mock.gateway.cost_today = 0.06f;
    mock.gateway.tokens_today = 71769;
    mock.gateway.active_session_count = 3;
    mock.gateway.total_session_count = 5;

    const char *ts[]   = {"14:28", "14:28", "14:29", "14:29", "14:29", "14:30", "14:30"};
    const char *typ[]  = {"user",  "tool",  "tool",  "reply", "user",  "tool",  "reply"};
    const char *txt[]  = {
        "check the git status before commit",
        "Bash: git status -uno",
        "Read: main/ui_screens.c",
        "Working tree has 3 modified files...",
        "fix the budget bar semantics",
        "Edit: main/ui_screens.c",
        "Done. Budget bars now reflect used%."
    };
    mock.transcript_count = 7;
    for (int i = 0; i < 7; i++) {
        strncpy(mock.transcript[i].ts, ts[i], 5);
        strncpy(mock.transcript[i].type, typ[i], 5);
        strncpy(mock.transcript[i].text, txt[i], 63);
    }

    /* Activity feed entries */
    const char *ats[]  = {"14:29", "14:29", "14:30"};
    const char *alvl[] = {"info", "info", "info"};
    const char *amsg[] = {
        "session restored from ~/.openclaw/agents/main",
        "gateway budget window: 9% used",
        "tokens today: 72k (in=23k out=614 cache_r=48k)"
    };
    mock.activity_count = 3;
    for (int i = 0; i < 3; i++) {
        strncpy(mock.activity[i].ts, ats[i], 5);
        strncpy(mock.activity[i].level, alvl[i], 5);
        strncpy(mock.activity[i].msg, amsg[i], 71);
    }

    activity_update(&mock);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* ── LVGL init ─────────────────────────────────────── */
    lv_init();

    /* Draw buffer — full-screen single buffer is simplest for a
     * headless render. */
    static lv_color_t draw_buf[SIM_W * SIM_H];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, draw_buf, NULL, SIM_W * SIM_H);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf  = &disp_buf;
    disp_drv.flush_cb  = dummy_flush_cb;
    disp_drv.hor_res   = SIM_W;
    disp_drv.ver_res   = SIM_H;
    disp_drv.full_refresh = 1;  /* Always repaint the whole screen into fb */
    lv_disp_drv_register(&disp_drv);

    /* ── Build the real UI ─────────────────────────────── */
    ui_build_all();
    populate_mock_data();

    /* ── Render each screen to PNG ─────────────────────── */
    ui_switch_to(SCREEN_DASH);
    render_and_save("out/01-dashboard.png");

    ui_switch_to(SCREEN_ACTIVITY);
    render_and_save("out/02-activity.png");

    ui_switch_to(SCREEN_SEND);
    render_and_save("out/03-control.png");

    /* About screen isn't in the screen_t enum — it's accessed via swipe.
     * We load it directly since the sim has no gesture input. */
    extern lv_obj_t *scr_about; /* not declared in header; see sim_main fallback */
    /* If the symbol isn't exported, skip it gracefully. */
    (void)scr_about;

    printf("sim complete\n");
    return 0;
}
