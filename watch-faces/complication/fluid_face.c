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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fluid_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "lis2dw.h"
#if defined(FORCE_CLASSIC_LCD_TYPE)
#include "fluid_face_classic_data.h"
#else
#include "fluid_face_data.h"
#endif

#define FLUID_TICK_FREQUENCY 8

// 2.4 volts seems to offer adequate warning of a low battery condition --
// same threshold and reasoning as clock_face's own.
#ifndef FLUID_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD
#define FLUID_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD 2400
#endif

// Grains a single segment can hold before it's full (see fluid_can_receive).
#define FLUID_SEGMENT_CAPACITY 2

// Peak total-acceleration magnitude, in g, RAW (non-filtered, so it includes the ~1G gravity
// baseline) -- a knock at or above this shatters the display. Unverified on real hardware --
// see fluid_face.h.
#define ACCEL_TRIGGER_G 2.5f

// Below this, the windowed X/Y/Z swing (see fluid_accel_variation_g) counts as "quiet" --
// used to detect when a shatter has settled enough to start reassembling.
#define ACCEL_QUIET_VARIATION_G 0.3f
#define ACCEL_RANGE LIS2DW_RANGE_4_G   // headroom above ACCEL_TRIGGER_G without clipping
#define ACCEL_DATA_RATE LIS2DW_DATA_RATE_100_HZ

// Raw-to-g conversion for ACCEL_RANGE. movement.c leaves the sensor in
// LIS2DW_MODE_LOW_POWER / LIS2DW_LP_MODE_1 (12-bit), so each raw sample is a signed 12-bit
// code (+/-2048) left-justified into the 16-bit register, with magnitude 2048 representing
// ACCEL_RANGE_G. So raw-counts-per-g = (2048 / ACCEL_RANGE_G) * 16 = 32768 / ACCEL_RANGE_G.
// Confirmed against hardware (resting flat: ~400 raw idle axis, ~8000 raw straight-down axis,
// both matching the formula's 8192 raw/g at 4g range). This replaces
// lis2dw_get_acceleration_measurement()'s own borrowed/unverified lsb-value table (see its
// FIXME), which also isn't usable on FIFO samples since it ignores its input and always
// re-reads the live registers instead.
#define ACCEL_RANGE_G 4.0f // must match ACCEL_RANGE
#define ACCEL_COUNTS_PER_G (32768.0f / ACCEL_RANGE_G)

// Confirmed on hardware: sensor +X points toward the top of the screen,
// sensor +Y points toward the right edge of the screen, sensor Z is the
// screen normal. Our own (x,y) convention is screen-right = +x, screen-
// DOWN = +y (matching the SVG data everything else is built from), so
// screen-right = sensor Y, and screen-down = -sensor X (since sensor +X is
// screen-up).
#define ACCEL_SCREEN_X(ax, ay) (ay)
#define ACCEL_SCREEN_Y(ax, ay) (-(ax))

// Combined X/Y magnitude below which a tilt reading is too small to trust (see
// fluid_tilt_direction) -- e.g. resting nearly flat, or the simulator where it's always 0.
#define ACCEL_TILT_DEADZONE_G 0.15f

#define QUIET_TICKS_REQUIRED (5 * FLUID_TICK_FREQUENCY) // 5 seconds of calm before reassembling
#define SETTLE_STEPS_PER_TICK 3                         // how many segments snap into place per tick while reassembling

// fluid_pixel_dir / fluid_dir_order are indexed 0..7 at 45-degree steps: 0=E, 1=SE, 2=S, 3=SW, 4=W, 5=NW, 6=N, 7=NE.
#define DIR_E 0
#define DIR_S 2
#define DIR_W 4
#define DIR_N 6
#define NUM_DIRS 8

// Pixel index of the colon. It's always 0 on custom: (com=0, seg=0) sorts first among all 92
// segments in fluid_face_data.h (see gen_fluid_header.py's sort key). On classic the colon is
// a different (com, seg) that doesn't happen to sort first, so fluid_face_classic_data.h
// #defines this one directly instead.
#if !defined(FORCE_CLASSIC_LCD_TYPE)
#define FLUID_COLON_PIXEL 0
#endif

// Pixel indices of the PM, 24H, SIGNAL/alarm, BELL/time-signal, and (custom LCD only)
// ARROWS/low-battery indicators, each com*23+seg (23 segs/COM line on custom). On classic,
// these five (minus ARROWS -- the module has no low-battery icon) are instead #defined
// directly in fluid_face_classic_data.h, since classic's pixel order has no such regularity.
#if !defined(FORCE_CLASSIC_LCD_TYPE)
#define FLUID_PM_PIXEL (3 * 23 + 21)
#define FLUID_24H_PIXEL (2 * 23 + 21)
#define FLUID_SIGNAL_PIXEL (0 * 23 + 21)
#define FLUID_BELL_PIXEL (1 * 23 + 21)
#define FLUID_ARROWS_PIXEL (2 * 23 + 0)
#endif

// Segment bits for a weekday abbreviation letter. Reuses Custom_LCD_Character_Set /
// Classic_LCD_Character_Set directly (same bit convention as fluid_digit_font) rather than
// hand-copying a subset that could drift out of sync with the real font.
static inline uint8_t fluid_weekday_char_bits(char c) {
#if defined(FORCE_CLASSIC_LCD_TYPE)
    return Classic_LCD_Character_Set[(uint8_t) c - 0x20];
#else
    return Custom_LCD_Character_Set[(uint8_t) c - 0x20];
#endif
}

static void fluid_set_char(uint8_t *out, const int8_t *pixels, int num_segs, uint8_t bits) {
    for (int seg = 0; seg < num_segs; seg++) {
        if (bits & (1 << seg)) {
            int8_t pixel = pixels[seg];
            if (pixel >= 0) out[pixel] = 1;
        }
    }
}

// Everything the display should show right now, as one pattern over the 92-pixel pool.
// Recomputed fresh every tick by both the clock and the settle-back animation, so the target
// keeps moving forward as real time passes even while reassembling.
static void fluid_compute_time_pattern(uint8_t *out, watch_date_time_t now, bool alarm_enabled, bool time_signal_enabled, bool battery_low) {
    uint8_t hour = now.unit.hour;
    bool is_12h = movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H;
    bool is_pm = is_12h && now.unit.hour >= 12;

    if (is_12h) {
        hour %= 12;
        if (hour == 0) hour = 12;
    }

    memset(out, 0, FLUID_NUM_PIXELS * sizeof(uint8_t));

    uint8_t digit_value[8] = { (uint8_t)(hour / 10), (uint8_t)(hour % 10),
                               (uint8_t)(now.unit.minute / 10), (uint8_t)(now.unit.minute % 10),
                               (uint8_t)(now.unit.second / 10), (uint8_t)(now.unit.second % 10),
                               (uint8_t)(now.unit.day / 10), (uint8_t)(now.unit.day % 10) };
    const int8_t *digit_pixels[8] = { fluid_hours_tens_pixel, fluid_hours_ones_pixel,
                                       fluid_minutes_tens_pixel, fluid_minutes_ones_pixel,
                                       fluid_seconds_tens_pixel, fluid_seconds_ones_pixel,
                                       fluid_day_tens_pixel, fluid_day_ones_pixel };
    // clock_face zero-pads hour and day only in MOVEMENT_CLOCK_MODE_024H ("24 hour clock with
    // leading zero") -- in plain 12H or 24H mode both use "%2d" (space-padded), so a tens
    // digit of 0 is a leading zero, not a real digit, and gets suppressed the same way here.
    bool zero_pad = movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H;
    bool suppress_hour_tens = !zero_pad && digit_value[0] == 0;
    bool suppress_day_tens = !zero_pad && digit_value[6] == 0;
    for (int d = 0; d < 8; d++) {
        if ((d == 0 && suppress_hour_tens) || (d == 6 && suppress_day_tens)) continue;
        fluid_set_char(out, digit_pixels[d], 7, fluid_digit_font[digit_value[d]]);
    }

#if defined(FORCE_CLASSIC_LCD_TYPE)
    const char *weekday = watch_utility_get_weekday(now); // "MO".."SU", classic only has 2 chars
    fluid_set_char(out, fluid_weekday1_pixel, 8, fluid_weekday_char_bits(weekday[0]));
    fluid_set_char(out, fluid_weekday2_pixel, 8, fluid_weekday_char_bits(weekday[1]));
#else
    const char *weekday = watch_utility_get_long_weekday(now); // "MON".."SUN", always 3 chars
    fluid_set_char(out, fluid_weekday1_pixel, 8, fluid_weekday_char_bits(weekday[0]));
    fluid_set_char(out, fluid_weekday2_pixel, 8, fluid_weekday_char_bits(weekday[1]));
    fluid_set_char(out, fluid_weekday3_pixel, 8, fluid_weekday_char_bits(weekday[2]));
#endif

    // Setting these as ordinary pixels in the pattern (rather than watch_set_indicator()) lets
    // them join the shatter/settle effects like every other segment; drawing any of them
    // separately would also get them silently wiped every tick, since fluid_redraw() redraws
    // every pixel in state->filled and this function's memset zeroes these addresses first.
    out[FLUID_COLON_PIXEL] = 1;
    out[FLUID_PM_PIXEL] = is_pm ? 1 : 0;
    out[FLUID_24H_PIXEL] = is_12h ? 0 : 1;
    out[FLUID_SIGNAL_PIXEL] = alarm_enabled ? 1 : 0;
    out[FLUID_BELL_PIXEL] = time_signal_enabled ? 1 : 0;
#if !defined(FORCE_CLASSIC_LCD_TYPE)
    out[FLUID_ARROWS_PIXEL] = battery_low ? 1 : 0; // no low-battery icon on the classic module
#else
    (void) battery_low;
#endif
}

// Checks the battery voltage at most once a day, same cadence as clock_face. Just updates
// the flag -- FLUID_ARROWS_PIXEL gets it from here via fluid_compute_time_pattern, same as
// every other bit of the displayed pattern.
static void fluid_check_battery_periodically(fluid_face_state_t *state, watch_date_time_t now) {
    if (now.unit.day == state->last_battery_check) return;
    state->last_battery_check = now.unit.day;
    state->battery_low = watch_get_vcc_voltage() < FLUID_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD;
}

// PM, 24H, SIGNAL, BELL, and ARROWS are drawn purely as pixels (see fluid_compute_time_pattern)
// -- ordinary members of the same 92-pixel pool this loop already redraws in full every call,
// so they join the shatter/settle effects like every other segment instead of sitting fixed
// on top of them via a separate watch_set_indicator(). A segment is lit whenever its count is
// nonzero, regardless of how close to FLUID_SEGMENT_CAPACITY it is -- capacity above 1 just
// lets a pile pack into fewer lit segments, for a denser, less busy look.
static void fluid_redraw(fluid_face_state_t *state) {
    for (int i = 0; i < FLUID_NUM_PIXELS; i++) {
        if (state->filled[i]) {
            watch_set_pixel(fluid_pixel_com[i], fluid_pixel_seg[i]);
        } else {
            watch_clear_pixel(fluid_pixel_com[i], fluid_pixel_seg[i]);
        }
    }
}

// Toggling only needs to touch the one pixel it affects (not recompute the whole pattern,
// which would clobber an in-progress shatter/reassembly) for immediate feedback; the next
// fluid_compute_time_pattern() call reasserts it going forward same as everything else.
static void fluid_toggle_time_signal(fluid_face_state_t *state) {
    state->time_signal_enabled = !state->time_signal_enabled;
    state->filled[FLUID_BELL_PIXEL] = state->time_signal_enabled ? 1 : 0;
    fluid_redraw(state);
}

// A neighbor can only accept an incoming grain if it exists and has room.
static inline bool fluid_can_receive(fluid_face_state_t *state, int8_t cell) {
    return cell >= 0 && state->filled[cell] < FLUID_SEGMENT_CAPACITY;
}

// Moves at most one grain out of pixel `from` toward whichever neighbor has room. If the
// primary-direction neighbor is full or off-screen, slides sideways (+/-90 degrees) instead,
// so a pile spreads out rather than jamming into a rigid column.
static void fluid_try_move(fluid_face_state_t *state, int from, int dir_index) {
    int8_t primary = fluid_pixel_dir[dir_index][from];
    if (fluid_can_receive(state, primary)) {
        state->filled[from]--;
        state->filled[primary]++;
        return;
    }

    int8_t side_a = fluid_pixel_dir[(dir_index + 2) % NUM_DIRS][from];
    int8_t side_b = fluid_pixel_dir[(dir_index + NUM_DIRS - 2) % NUM_DIRS][from];
    bool a_open = fluid_can_receive(state, side_a);
    bool b_open = fluid_can_receive(state, side_b);

    if (a_open && (!b_open || (from & 1))) {
        state->filled[from]--;
        state->filled[side_a]++;
    } else if (b_open) {
        state->filled[from]--;
        state->filled[side_b]++;
    }
}

// Each cell moves at most one grain this tick, however many (up to
// FLUID_SEGMENT_CAPACITY) it's currently holding.
static void fluid_step(fluid_face_state_t *state, int dir_index) {
    const uint8_t *order = fluid_dir_order[dir_index];
    for (int oi = 0; oi < FLUID_NUM_PIXELS; oi++) {
        int i = order[oi];
        if (state->filled[i] > 0) fluid_try_move(state, i, dir_index);
    }
}

// Snaps up to SETTLE_STEPS_PER_TICK mismatched grains into place. Returns true once the
// display fully matches the current time. The target is always a plain 0/1 pattern, so a cell
// sitting at FLUID_SEGMENT_CAPACITY may take an extra tick or two to fully drain back to it.
static bool fluid_settle_step(fluid_face_state_t *state) {
    uint8_t target[FLUID_NUM_PIXELS];
    fluid_compute_time_pattern(target, movement_get_local_date_time(), movement_alarm_enabled(), state->time_signal_enabled, state->battery_low);

    int budget = SETTLE_STEPS_PER_TICK;
    bool mismatch = false;

    // "Done" means the lit/unlit pattern matches, not that every count is
    // exactly equal -- a cell sitting at 2 when the target only asks for
    // "lit" looks identical to one sitting at 1, and whatever's left over
    // gets wiped clean the moment FLUID_MODE_CLOCK starts overwriting
    // fluid_compute_time_pattern() fresh every tick anyway.
    for (int i = 0; i < FLUID_NUM_PIXELS && budget > 0; i++) {
        if (state->filled[i] > 0 && target[i] == 0) {
            state->filled[i]--;
            budget--;
        }
    }
    for (int i = 0; i < FLUID_NUM_PIXELS && budget > 0; i++) {
        if (state->filled[i] == 0 && target[i] > 0) {
            state->filled[i]++;
            budget--;
        }
    }
    for (int i = 0; i < FLUID_NUM_PIXELS; i++) {
        if ((state->filled[i] > 0) != (target[i] > 0)) {
            mismatch = true;
            break;
        }
    }

    return !mismatch;
}

static inline float fluid_raw_to_g(int16_t raw) {
    return (float) raw / ACCEL_COUNTS_PER_G;
}

// Reads every sample buffered since the last call. Returns the largest total-acceleration
// magnitude among them in g (peeking at only the latest sample would miss a brief shock
// between ticks), and writes the *last* sample's X/Y/Z to *out_x/*out_y/*out_z for
// direction/quiet purposes, which want recent data, not the peak. Returns false (leaving the
// outputs untouched) if the FIFO had nothing new -- callers must not treat that as (0,0,0),
// which would corrupt anything reading the actual values, like the quiet-variation window.
static bool fluid_read_peak_g(float *out_x, float *out_y, float *out_z, float *out_peak) {
    lis2dw_fifo_t fifo = {0};
    // Return value is FIFO_SAMPLE_OVERRUN: the 32-sample FIFO filled up and started
    // overwriting its oldest entries before we got here, possibly losing a knock's true peak.
    // Nothing to recover after the fact; the mitigation is polling often enough that 32
    // samples (320ms at 100Hz) can't fill between reads. FLUID_MODE_CLOCK's 1Hz tick (battery
    // life, see fluid_set_mode) means a knock in the ~680ms gap it can't cover may go
    // undetected while idle; FLUID_MODE_FLUID/SETTLING poll fast enough not to have this issue.
    lis2dw_read_fifo(&fifo);
    lis2dw_clear_fifo();

    float peak = 0;
    for (int i = 0; i < fifo.count; i++) {
        float x = fluid_raw_to_g(fifo.readings[i].x);
        float y = fluid_raw_to_g(fifo.readings[i].y);
        float z = fluid_raw_to_g(fifo.readings[i].z);
        float mag2 = x * x + y * y + z * z;
        if (mag2 > peak * peak) peak = sqrtf(mag2);
        if (i == fifo.count - 1) {
            *out_x = x;
            *out_y = y;
            *out_z = z;
        }
    }

    *out_peak = peak;
    return fifo.count > 0;
}

static float fluid_window_range(const float *window, uint8_t count) {
    float lo = window[0];
    float hi = window[0];
    for (uint8_t i = 1; i < count; i++) {
        if (window[i] < lo) lo = window[i];
        if (window[i] > hi) hi = window[i];
    }
    return hi - lo;
}

// If have_sample, pushes this tick's X/Y/Z into their rolling 1-second windows (skipped when
// there was no new FIFO data -- a hole in the window is fine, but a fake (0,0,0) sample would
// look like a huge spurious swing). Either way, returns the sum of how much each axis has
// swung (max-min) over the window. Unlike a magnitude check, this catches pure rotation: |g|
// stays pinned at ~1G while spinning, but the individual X/Y/Z components swing as that
// fixed-length vector points a different way each moment.
static float fluid_accel_variation_g(fluid_face_state_t *state, bool have_sample, float x, float y, float z) {
    if (have_sample) {
        state->accel_window_x[state->accel_window_pos] = x;
        state->accel_window_y[state->accel_window_pos] = y;
        state->accel_window_z[state->accel_window_pos] = z;
        state->accel_window_pos = (state->accel_window_pos + 1) % FLUID_ACCEL_WINDOW_LEN;
        if (state->accel_window_count < FLUID_ACCEL_WINDOW_LEN) state->accel_window_count++;
    }
    if (state->accel_window_count == 0) return 0;

    return fluid_window_range(state->accel_window_x, state->accel_window_count)
         + fluid_window_range(state->accel_window_y, state->accel_window_count)
         + fluid_window_range(state->accel_window_z, state->accel_window_count);
}

// Which of the 8 fall directions best matches the watch's current tilt, or
// -1 if the tilt reading is too small to trust (see ACCEL_TILT_DEADZONE_G --
// notably always true in the simulator). Below that deadzone, atan2f's
// answer is dominated by noise, so callers should keep whatever direction
// was last known good rather than accept it.
static int fluid_tilt_direction(float raw_x, float raw_y) {
    float sx = ACCEL_SCREEN_X(raw_x, raw_y);
    float sy = ACCEL_SCREEN_Y(raw_x, raw_y);
    if ((sx * sx + sy * sy) < (ACCEL_TILT_DEADZONE_G * ACCEL_TILT_DEADZONE_G)) {
        return -1;
    }
    float angle_deg = atan2f(sy, sx) * (180.0f / 3.14159265f);
    int dir = ((int) lroundf(angle_deg / 45.0f)) % NUM_DIRS;
    if (dir < 0) dir += NUM_DIRS;
    return dir;
}

// Requests the tick rate `mode` needs and switches to it: FLUID_MODE_CLOCK only redraws
// once a second, but FLUID_MODE_FLUID/FLUID_MODE_SETTLING need FLUID_TICK_FREQUENCY to
// poll the accelerometer and animate smoothly.
static void fluid_set_mode(fluid_face_state_t *state, fluid_mode_t mode) {
    state->mode = mode;
    movement_request_tick_frequency(mode == FLUID_MODE_CLOCK ? 1 : FLUID_TICK_FREQUENCY);
}

static void fluid_enter_fluid_mode(fluid_face_state_t *state) {
    fluid_set_mode(state, FLUID_MODE_FLUID);
    state->quiet_ticks = 0;
}

void fluid_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(fluid_face_state_t));
        memset(*context_ptr, 0, sizeof(fluid_face_state_t));
    }
}

void fluid_face_activate(void *context) {
    fluid_face_state_t *state = (fluid_face_state_t *) context;

    // The sensor's range/filter/background-rate are global registers shared with every
    // other accelerometer-using face -- save them so fluid_face_resign can put them back.
    state->saved_accel_range = lis2dw_get_range();
    state->saved_accel_filter = lis2dw_get_filter_type();
    state->saved_accel_background_rate = movement_get_accelerometer_background_rate();

    movement_set_accelerometer_background_rate(ACCEL_DATA_RATE);
    lis2dw_set_range(ACCEL_RANGE);
    // Raw + gravity, not the sensor's own high-pass-filtered wake-up path: that filter bit is a
    // global switch that also filters the OUT_X/Y/Z data our FIFO reads use, which would break
    // tilt sensing and fluid_accel_variation_g's quiet check, and its response felt duller anyway.
    lis2dw_set_filter_type(LIS2DW_FILTER_LOW_PASS);
    lis2dw_enable_fifo();
    lis2dw_clear_fifo();

    state->quiet_ticks = 0;
    state->manual_recovery = false;
    state->accel_window_pos = 0;
    state->accel_window_count = 0;
    state->last_dir_index = DIR_S; // one-time initial guess, not a per-tick fallback
    fluid_set_mode(state, FLUID_MODE_CLOCK);
    fluid_check_battery_periodically(state, movement_get_local_date_time());
    fluid_compute_time_pattern(state->filled, movement_get_local_date_time(), movement_alarm_enabled(), state->time_signal_enabled, state->battery_low);
    // First draw happens in EVENT_ACTIVATE below, which always follows immediately.
}

bool fluid_face_loop(movement_event_t event, void *context) {
    fluid_face_state_t *state = (fluid_face_state_t *) context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            fluid_redraw(state);
            break;
        case EVENT_TICK: {
            fluid_check_battery_periodically(state, movement_get_local_date_time());

            float tilt_x = 0, tilt_y = 0, tilt_z = 0, peak_g = 0;
            bool have_sample = fluid_read_peak_g(&tilt_x, &tilt_y, &tilt_z, &peak_g);
            float variation_g = fluid_accel_variation_g(state, have_sample, tilt_x, tilt_y, tilt_z);
            bool quiet_now = variation_g <= ACCEL_QUIET_VARIATION_G;
            int measured_dir = fluid_tilt_direction(tilt_x, tilt_y);
            if (measured_dir >= 0) state->last_dir_index = (int8_t) measured_dir; // else: too weak to trust, keep the last one
            int dir_index = state->last_dir_index;

            bool shock_detected = peak_g >= ACCEL_TRIGGER_G;

            // Skip all of this while manually recovering: residual physical motion right
            // after the shake that caused the shatter would otherwise immediately re-trigger
            // shock_detected or the "still shaky" branch below and undo the forced settle.
            if (!state->manual_recovery) {
                if (shock_detected) {
                    fluid_enter_fluid_mode(state);
                } else if (!quiet_now) {
                    if (state->mode == FLUID_MODE_SETTLING) {
                        // still shaky -- abandon the half-finished reassembly and re-shatter.
                        fluid_enter_fluid_mode(state);
                    } else if (state->mode == FLUID_MODE_FLUID) {
                        state->quiet_ticks = 0;
                    }
                } else if (state->mode == FLUID_MODE_FLUID) {
                    state->quiet_ticks++;
                    if (state->quiet_ticks >= QUIET_TICKS_REQUIRED) {
                        fluid_set_mode(state, FLUID_MODE_SETTLING);
                    }
                }
            }

            switch (state->mode) {
                case FLUID_MODE_CLOCK:
                    fluid_compute_time_pattern(state->filled, movement_get_local_date_time(), movement_alarm_enabled(), state->time_signal_enabled, state->battery_low);
                    break;
                case FLUID_MODE_FLUID:
                    fluid_step(state, dir_index);
                    break;
                case FLUID_MODE_SETTLING:
                    if (fluid_settle_step(state)) {
                        state->manual_recovery = false; // recovery complete, resume normal sensing
                        fluid_set_mode(state, FLUID_MODE_CLOCK);
                    }
                    break;
            }

            fluid_redraw(state);
            break;
        }
        case EVENT_ALARM_BUTTON_UP:
            // Manual toggle for testing, since the simulator has no
            // accelerometer: shatter, or -- if already shattered -- trigger
            // the return early instead of waiting out the quiet timer.
            if (state->mode == FLUID_MODE_FLUID) {
                state->manual_recovery = true;
                fluid_set_mode(state, FLUID_MODE_SETTLING);
                watch_buzzer_play_note(BUZZER_NOTE_C8, 50); // confirms the button (not shake) triggered this recovery
            } else {
                state->manual_recovery = false;
                fluid_enter_fluid_mode(state);
            }
            fluid_redraw(state);
            break;
        case EVENT_MODE_BUTTON_UP:
            movement_move_to_next_face();
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // I2C is disabled during this event, so no accelerometer polling -- fall back to a
            // plain ticking clock and drop out of FLUID/SETTLING if mid-effect when sleep started.
            state->mode = FLUID_MODE_CLOCK;
            state->manual_recovery = false;
            fluid_compute_time_pattern(state->filled, movement_get_local_date_time(), movement_alarm_enabled(), state->time_signal_enabled, state->battery_low);
            fluid_redraw(state);
            break;
        case EVENT_ALARM_LONG_PRESS:
            fluid_toggle_time_signal(state);
            break;
        case EVENT_BACKGROUND_TASK:
            movement_play_signal();
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

movement_watch_face_advisory_t fluid_face_advise(void *context) {
    movement_watch_face_advisory_t retval = { 0 };
    fluid_face_state_t *state = (fluid_face_state_t *) context;

    if (state->time_signal_enabled) {
        watch_date_time_t date_time = movement_get_local_date_time();
        retval.wants_background_task = date_time.unit.minute == 0;
    }

    return retval;
}

void fluid_face_resign(void *context) {
    fluid_face_state_t *state = (fluid_face_state_t *) context;
    movement_request_tick_frequency(1);
    lis2dw_clear_fifo();
    lis2dw_disable_fifo();
    // Put the shared sensor registers back the way fluid_face_activate found them.
    lis2dw_set_range(state->saved_accel_range);
    lis2dw_set_filter_type(state->saved_accel_filter);
    movement_set_accelerometer_background_rate(state->saved_accel_background_rate);
}
