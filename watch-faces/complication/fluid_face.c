/* SPDX-License-Identifier: MIT */

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

// How many "grains" a single physical segment can hold before it's full and
// can't accept any more. >1 lets a pile pack into fewer distinct lit
// segments (denser, visually less busy) for the same total amount of
// liquid; the segment itself still only has ON/OFF to show, so it's lit
// whenever it holds anything at all, regardless of how close to full.
#define FLUID_SEGMENT_CAPACITY 2

// How hard a knock has to be to shatter the display, as RAW (non-filtered)
// total-acceleration magnitude. We tried the sensor's own hardware wake-up
// comparator (high-pass filtered, gravity subtracted out) for this, but:
// (a) FDS (the filter-type bit) turned out to be a global switch that also
// high-pass-filters the normal OUT_X/Y/Z data our own FIFO reads come from,
// which broke tilt-direction sensing (needs gravity's DC component intact)
// and the quiet-variation check below; and (b) even taken on its own, the
// hardware comparator's response felt noticeably duller/less sensitive than
// doing it ourselves. So this is back to a plain magnitude threshold, which
// means it has to clear the ~1G baseline from gravity as well as the actual
// knock -- see ACCEL_QUIET_VARIATION_G's comment. Unverified on real
// hardware -- see fluid_face.h.
#define ACCEL_TRIGGER_G 2.5f

// A watch just sitting still still reads ~1G the whole time (gravity), so
// "quiet" can't be "magnitude below X" -- confirmed on hardware, it never
// dips low enough for that to fire. It also can't be "the combined
// magnitude isn't changing": spinning the watch keeps |g| pinned at ~1G
// even though it's clearly in motion, since gravity's magnitude doesn't
// care about orientation, only its direction does. So "quiet" instead means
// the sum of how much X, Y, and Z have each individually swung (max-min)
// over the last second doesn't exceed this -- rotation shows up here even
// when it doesn't in the combined magnitude. This is a different quantity
// than the single-channel version this constant used to gate, so expect to
// retune it.
#define ACCEL_QUIET_VARIATION_G 0.3f
#define ACCEL_RANGE LIS2DW_RANGE_4_G   // headroom above ACCEL_TRIGGER_G without clipping
#define ACCEL_DATA_RATE LIS2DW_DATA_RATE_100_HZ

// Raw-to-g conversion for ACCEL_RANGE. movement.c leaves the sensor in
// LIS2DW_MODE_LOW_POWER / LIS2DW_LP_MODE_1 (12-bit) at startup and fluid_face
// never changes that, so each raw sample is a signed 12-bit code (+/-2048)
// left-justified into the 16-bit register (i.e. shifted left 4 bits), with
// magnitude 2048 representing ACCEL_RANGE_G worth of acceleration. So
// raw-counts-per-g = (2048 / ACCEL_RANGE_G) * 16 = 32768 / ACCEL_RANGE_G.
// Confirmed against hardware: resting flat read ~400 raw on the idle axis,
// ~8000 raw on the axis facing straight down -- both consistent with this
// formula's prediction of 8192 raw per g at a 4g range. This replaces
// lis2dw_get_acceleration_measurement()'s own borrowed/unverified lsb-value
// table (see its FIXME), which also isn't usable directly on FIFO samples
// since it ignores its input and always re-reads the live (non-FIFO)
// registers instead.
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

// Below this combined X/Y magnitude, treat the tilt reading as too small to
// trust (e.g. resting nearly flat, or the simulator where it's always 0) --
// callers keep the last known-good direction instead of accepting it.
#define ACCEL_TILT_DEADZONE_G 0.15f

#define QUIET_TICKS_REQUIRED (5 * FLUID_TICK_FREQUENCY) // 5 seconds of calm before reassembling
#define SETTLE_STEPS_PER_TICK 3                         // how many segments snap into place per tick while reassembling

// fluid_pixel_dir / fluid_dir_order are indexed 0..7 at 45-degree steps: 0=E, 1=SE, 2=S, 3=SW, 4=W, 5=NW, 6=N, 7=NE.
#define DIR_E 0
#define DIR_S 2
#define DIR_W 4
#define DIR_N 6
#define NUM_DIRS 8

// The colon (com=0, seg=0) sorts first among all 92 segments in
// fluid_face_data.h (see gen_fluid_header.py's sort key), so it's always
// index 0. It joins the simulation like any other segment now -- no special
// handling needed beyond seeding it into the target pattern.
//
// On classic the colon is a different (com, seg) that doesn't happen to sort
// first, so fluid_face_classic_data.h #defines this one directly instead.
#if !defined(FORCE_CLASSIC_LCD_TYPE)
#define FLUID_COLON_PIXEL 0
#endif

// PM, 24H, SIGNAL/alarm, BELL/time-signal, and (custom LCD only) ARROWS/low-battery
// indicators, all just ordinary pixels in the target pattern now -- previously PM/24H were
// drawn separately via watch_set_indicator() after the fact, which both kept them
// fixed/unaffected by the shatter and fluid_step() effects (defeating the point of those)
// and, worse, got silently wiped every tick by the grain loop redrawing every *other* pixel
// these addresses are also part of. SIGNAL/BELL/ARROWS had the exact same bug (fluid_redraw()
// draws every pixel from state->filled every tick, and fluid_compute_time_pattern's memset
// zeroes all of them first) but were never folded into the pattern when PM/24H were fixed, so
// the alarm, chime, and low-battery icons were silently cleared again on the very next tick
// after being set -- making it look (and, via EVENT_ALARM_LONG_PRESS's fluid_toggle_time_signal,
// *feel*) like the chime setting wasn't sticking, even though the underlying
// state->time_signal_enabled it actually acts on was unaffected the whole time.
//
// On classic, these five (minus ARROWS -- the module has no low-battery icon) are instead
// #defined directly in fluid_face_classic_data.h: com*23+seg is a custom-LCD-specific
// pixel-index formula (23 segs/COM line there; classic's pixel order has no such regularity,
// so its data header hands over plain literal indices instead).
#if !defined(FORCE_CLASSIC_LCD_TYPE)
#define FLUID_PM_PIXEL (3 * 23 + 21)
#define FLUID_24H_PIXEL (2 * 23 + 21)
#define FLUID_SIGNAL_PIXEL (0 * 23 + 21)
#define FLUID_BELL_PIXEL (1 * 23 + 21)
#define FLUID_ARROWS_PIXEL (2 * 23 + 0)
#endif

// Segment bits for weekday abbreviation letters, always uppercase A-Z from
// watch_utility_get_long_weekday() ("MON".."SUN", custom LCD -- 3 characters) or
// watch_utility_get_weekday() ("MO".."SU", classic -- 2 characters, see
// fluid_compute_time_pattern). Custom_LCD_Character_Set / Classic_LCD_Character_Set
// (watch_common_display.h) are already indexed by character - 0x20 with this exact bit
// convention (bit 0 = segment A .. bit 7 = segment H, same as fluid_digit_font), so reuse
// them directly rather than hand-copying a subset that could silently drift out of sync
// with the real font.
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

// Everything the display should show for the current moment: hours,
// minutes, seconds, weekday, day of month, the colon, and the PM/24H
// indicators, all as one pattern over the same 92-pixel pool. Recomputed
// fresh every tick (by both the ordinary
// clock and the settle-back animation), so it always reflects whatever
// time it is *right now* -- including while reassembling, so the target
// itself keeps moving forward as real seconds (and occasionally weekday/
// day) pass.
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
// on top of them via a separate watch_set_indicator().
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

// Move (at most) one grain out of pixel `from`, one step in the given
// direction (0..7, see the DIR_* constants), into whichever neighbor has
// room. If the primary-direction neighbor is full (or doesn't exist, i.e.
// we're at the edge of the display), try sliding sideways (+/-90 degrees
// from the fall direction) instead, so a pile spreads out rather than
// jamming into a single rigid column. A cell holding 2 grains only ever
// sends one of them per tick -- see fluid_step.
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

// Snap up to SETTLE_STEPS_PER_TICK mismatched grains into place. Returns
// true once the display fully matches the current time. The target is
// always a plain 0/1 pattern (a normal digit display never doubles up), so
// a cell sitting at FLUID_SEGMENT_CAPACITY may take an extra tick or two to
// fully drain back to it.
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

// Reads every sample the accelerometer has buffered since the last call.
// Returns the largest total-acceleration magnitude among them, in g (peeking
// at only the latest sample would miss a brief shock between ticks, so we
// scan the whole FIFO for this); also writes the X/Y/Z of the *last* (most
// recent) sample to *out_x/*out_y/*out_z, for direction/quiet purposes --
// unlike the peak, those should be recent, not the biggest. Returns false
// (and leaves *out_x/*out_y/*out_z untouched) if the FIFO had nothing new --
// always true on the simulator (no I2C hardware), but also possible on real
// hardware if a tick happens to land between samples. Callers must not
// treat that as "reading (0,0,0)": that's nowhere near a real sample (resting
// still reads ~1G on some axis) and would corrupt anything that looks at
// the actual values, like the quiet-variation window below.
static bool fluid_read_peak_g(float *out_x, float *out_y, float *out_z, float *out_peak) {
    lis2dw_fifo_t fifo = {0};
    // The return value is the sensor's FIFO_SAMPLE_OVERRUN bit (see lis2dw.c): the 32-
    // sample FIFO filled up completely and started overwriting its own oldest entries
    // before we got here, so some samples -- possibly including the true peak of a knock
    // -- are already gone by the time we read. There's nothing to recover after the fact
    // (every other caller of lis2dw_read_fifo in this codebase discards it too), and the
    // only real mitigation is polling often enough that 32 samples (320ms at 100Hz) can't
    // fill up between reads. Note this is a known tradeoff of FLUID_MODE_CLOCK now
    // ticking at 1Hz instead of FLUID_TICK_FREQUENCY (see fluid_set_mode) for battery
    // life: a knock landing in the ~680ms gap the FIFO can't cover may go undetected while
    // idle on the plain clock display. FLUID_MODE_FLUID/SETTLING still poll fast enough
    // not to have this problem.
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

// If have_sample, pushes this tick's X/Y/Z into their rolling 1-second
// windows first (skip that when there was no new FIFO data this tick --
// see fluid_read_peak_g -- a hole in the window is fine, but a fake (0,0,0)
// sample is not: it looks like a huge, spurious swing). Either way, returns
// the sum of how much each axis has swung (max-min) over the window as it
// now stands. Unlike a single magnitude-variation check, this catches pure
// rotation: spinning the watch keeps |g| pinned at ~1G throughout (gravity's
// magnitude doesn't care about orientation), but the individual X/Y/Z
// components swing substantially as that fixed-length vector points a
// different way each moment, so their sum does not stay flat the way the
// magnitude does.
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
    lis2dw_set_filter_type(LIS2DW_FILTER_LOW_PASS); // raw+gravity data -- see ACCEL_TRIGGER_G's comment
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
    // First draw happens in response to EVENT_ACTIVATE below, not here --
    // it always follows immediately, so drawing here too would just be the
    // same frame rendered twice.
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
            // All peripherals but the RTC (I2C included) are disabled during
            // this event, so no accelerometer polling here. Show a plain
            // ticking clock instead -- fluid_compute_time_pattern/
            // fluid_redraw only ever touch the RTC and the LCD, both fine.
            // A long sleep is as good a reason as any to consider it
            // already "settled", so drop out of FLUID/SETTLING if we were
            // mid-effect when sleep started.
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
