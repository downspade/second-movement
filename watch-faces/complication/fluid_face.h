/* SPDX-License-Identifier: MIT */

#ifndef FLUID_FACE_H_
#define FLUID_FACE_H_

/*
 * FLUID FACE (custom LCD only)
 *
 * Normally shows a plain digital clock -- hours, minutes, seconds, weekday,
 * day of month, and the colon. A hard knock (real accelerometer shock on
 * hardware; the ALARM button stands in for it here, since the simulator has
 * no accelerometer) shatters ALL of that into a falling-sand "liquid" that
 * piles up in whichever direction the watch is currently tilted. Once
 * things go quiet for 5 seconds, the pile reassembles into the current
 * time over a couple of seconds -- seconds (and weekday/day, if a boundary
 * happens to pass) keep advancing throughout, since the target it's
 * reassembling into is recomputed fresh every tick. The PM/24H/BELL
 * indicators are the only things that don't join the effect.
 *
 * Real hardware calibration is unverified (see the ACCEL_* constants in
 * fluid_face.c) -- expect to retune the trigger/quiet thresholds and the
 * tilt-direction axis mapping by testing on an actual watch, both because
 * "how hard is 2G" is inherently a physical question and because lis2dw.c's
 * g-unit conversion is known-imprecise (see CLAUDE.md).
 *
 * See fluid_face_data.h for the per-segment position/adjacency data this
 * is built from (generated from watch-library/simulator/shell.html's
 * custom-LCD artwork).
 *
 * Press MODE to leave. Press ALARM to manually toggle shatter/return (this
 * is the only way to see it happen in the simulator); long-press ALARM to
 * toggle the hourly chime, same as clock_face.
 */

#include "movement.h"
#include "lis2dw.h"

#define FLUID_NUM_PIXELS 92
#define FLUID_ACCEL_WINDOW_LEN 8 // must match FLUID_TICK_FREQUENCY in fluid_face.c (1 second of samples)

typedef enum {
    FLUID_MODE_CLOCK,     // ordinary ticking clock
    FLUID_MODE_FLUID,     // shattered, falling/settling under gravity
    FLUID_MODE_SETTLING,  // reassembling into the current time
} fluid_mode_t;

typedef struct {
    // 0 = empty, up to FLUID_SEGMENT_CAPACITY (see fluid_face.c) -- each
    // physical segment can hold more than one "grain" of liquid, so a dense
    // pile doesn't have to light up as many distinct segments to hold the
    // same total amount. Lit for display whenever this is > 0.
    uint8_t filled[FLUID_NUM_PIXELS];
    fluid_mode_t mode;
    uint16_t quiet_ticks; // consecutive ticks with no shake, while in FLUID_MODE_FLUID
    // Sitting still still reads ~1G (gravity), so "quiet" can't mean "low
    // magnitude" -- and it can't mean "magnitude isn't changing" either:
    // spinning the watch keeps the magnitude of gravity fixed at ~1G even
    // though it's very much in motion, only its direction changes. So this
    // tracks each of the X/Y/Z components separately (a rolling 1-second
    // window per axis) -- rotation shows up there even when the combined
    // magnitude doesn't.
    float accel_window_x[FLUID_ACCEL_WINDOW_LEN];
    float accel_window_y[FLUID_ACCEL_WINDOW_LEN];
    float accel_window_z[FLUID_ACCEL_WINDOW_LEN];
    uint8_t accel_window_pos;
    uint8_t accel_window_count;
    // PM/24H redraw only when this (a combined clock-mode + AM/PM key, see
    // fluid_draw_indicators) actually changes, instead of every tick, to
    // avoid visibly flickering it for no reason.
    int8_t indicator_key; // -1 = not drawn yet
    // Last fall direction (0..7, see the DIR_* constants in fluid_face.c).
    // When the tilt reading is too weak to trust, we keep this instead of
    // forcing a default -- there's no reason to believe "down" over
    // whatever direction it was already falling.
    int8_t last_dir_index;
    // Hourly chime, same as clock_face's ALARM long-press toggle. Resets to
    // false on every reboot (only initialized when first allocated).
    bool time_signal_enabled;
    // Low-battery warning, checked once a day like clock_face's own.
    uint8_t last_battery_check; // day of month it was last checked, 0 = never
    bool battery_low;
    // The sensor's range/filter/background-rate are global registers shared with every
    // other accelerometer-using face (e.g. lis2dw_monitor_face, activity_logging_face).
    // Saved on activate and put back on resign, so leaving this face doesn't silently
    // leave the sensor at fluid_face's own settings for everyone else.
    lis2dw_range_t saved_accel_range;
    lis2dw_filter_t saved_accel_filter;
    lis2dw_data_rate_t saved_accel_background_rate;
} fluid_face_state_t;

void fluid_face_setup(uint8_t watch_face_index, void ** context_ptr);
void fluid_face_activate(void *context);
bool fluid_face_loop(movement_event_t event, void *context);
void fluid_face_resign(void *context);
movement_watch_face_advisory_t fluid_face_advise(void *context);

#define fluid_face ((const watch_face_t) { \
    fluid_face_setup, \
    fluid_face_activate, \
    fluid_face_loop, \
    fluid_face_resign, \
    fluid_face_advise, \
})

#endif // FLUID_FACE_H_
