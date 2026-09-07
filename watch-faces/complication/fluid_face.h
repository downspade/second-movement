/*
 * MIT License
 *
 * Copyright (c) 2026 Downspade
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FLUID_FACE_H_
#define FLUID_FACE_H_

/*
 * FLUID FACE
 *
 * A plain digital clock that a hard knock (accelerometer shock; ALARM button in the
 * simulator) shatters into a falling-sand "liquid" piling up in whichever direction the watch
 * is tilted. After 5 quiet seconds, it reassembles into the current time -- the target keeps
 * advancing throughout, since it's recomputed fresh every tick. PM/24H, the alarm bell, the
 * hourly chime, and (custom LCD only) the low-battery warning are baked into the same pattern,
 * so they join the effect too.
 *
 * Real hardware calibration is unverified (see the ACCEL_* constants in fluid_face.c) --
 * expect to retune the trigger/quiet thresholds and tilt-direction mapping on an actual watch,
 * since lis2dw.c's g-unit conversion is known-imprecise (see CLAUDE.md).
 *
 * Supports both display types via FORCE_CUSTOM_LCD_TYPE/FORCE_CLASSIC_LCD_TYPE. See
 * fluid_face_data.h (custom) and fluid_face_classic_data.h (classic) for the per-segment
 * position/adjacency data each is built from, generated from watch-library/simulator/
 * shell.html's segment artwork (plus, for classic, watch_common_display.h's
 * Classic_LCD_Display_Mapping, since classic's segments alias/omit some font bits the artwork
 * alone doesn't reveal).
 *
 * Press MODE to leave. Press ALARM to manually toggle shatter/return; long-press ALARM to
 * toggle the hourly chime, same as clock_face.
 */

#include "movement.h"
#include "lis2dw.h"

#if defined(FORCE_CLASSIC_LCD_TYPE)
#define FLUID_NUM_PIXELS 72
#else
#define FLUID_NUM_PIXELS 92
#endif
#define FLUID_ACCEL_WINDOW_LEN 8 // must match FLUID_TICK_FREQUENCY in fluid_face.c (1 second of samples)

typedef enum {
    FLUID_MODE_CLOCK,     // ordinary ticking clock
    FLUID_MODE_FLUID,     // shattered, falling/settling under gravity
    FLUID_MODE_SETTLING,  // reassembling into the current time
} fluid_mode_t;

typedef struct {
    // 0 = empty, up to FLUID_SEGMENT_CAPACITY (see fluid_face.c). Lit for display when > 0.
    uint8_t filled[FLUID_NUM_PIXELS];
    fluid_mode_t mode;
    // True from the moment Alarm forces FLUID_MODE_SETTLING (skipping the quiet timer) until
    // that settle finishes. While true, EVENT_TICK's accelerometer check is skipped, so
    // residual motion right after the shake can't immediately undo the manual recovery.
    bool manual_recovery;
    uint16_t quiet_ticks; // consecutive ticks with no shake, while in FLUID_MODE_FLUID
    // Rolling 1-second window per axis, since gravity's magnitude alone can't detect "quiet"
    // (spinning keeps |g| pinned at ~1G) -- see ACCEL_QUIET_VARIATION_G in fluid_face.c.
    float accel_window_x[FLUID_ACCEL_WINDOW_LEN];
    float accel_window_y[FLUID_ACCEL_WINDOW_LEN];
    float accel_window_z[FLUID_ACCEL_WINDOW_LEN];
    uint8_t accel_window_pos;
    uint8_t accel_window_count;
    // Last fall direction (0..7, see the DIR_* constants in fluid_face.c). Kept as-is when the
    // tilt reading is too weak to trust, rather than forcing a default direction.
    int8_t last_dir_index;
    bool time_signal_enabled; // hourly chime; resets false on every reboot
    uint8_t last_battery_check; // day of month it was last checked, 0 = never
    bool battery_low;
    // The sensor's range/filter/background-rate are global registers shared with every other
    // accelerometer-using face. Saved on activate, restored on resign.
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
