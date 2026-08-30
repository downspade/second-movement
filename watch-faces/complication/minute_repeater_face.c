/* SPDX-License-Identifier: MIT */

/*
 * MIT License
 *
 * Copyright (c) 2023 Jonas Termeau
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
#include "minute_repeater_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"

// 2.2 volts is roughly 5-10% battery remaining -- same threshold as the
// original repetition_minute_face this was ported from.
#ifndef MINUTE_REPEATER_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD
#define MINUTE_REPEATER_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD 2200
#endif

// watch_buzzer_play_note() is NOT blocking: it just hands one note off to a
// timer/interrupt-driven player and returns immediately (see watch_tcc.c on
// both hardware and the simulator), and each call starts by aborting
// whatever sequence is already playing. Calling it repeatedly in a loop (the
// original repetition_minute_face's approach) means every call aborts the
// previous note before it's ever heard -- true on real hardware too, not
// just the simulator. The correct way to play more than one note is to
// build a single sequence (note/duration-in-ticks pairs, see watch_tcc.h)
// and hand the whole thing to watch_buzzer_play_sequence() once, same as
// counter_face.c's beep_counter(). A negative pair count N followed by a
// repeat count replays the preceding |N| pairs that many extra times,
// which keeps a long run of identical chimes down to a few bytes.
#define MINUTE_REPEATER_TICKS(ms) ((int8_t)((ms) / 15))

// Chimes out the current time, Howdy neighbors: like an actual (very
// expensive) watch with a minute repeater complication, it's boring at
// 00:00 or 1:00 and very musical at 23:59 or 12:59.
static void minute_repeater_chime_time(watch_date_time_t date_time) {
    int hours = date_time.unit.hour;
    int quarters = date_time.unit.minute / 15;
    int minutes = date_time.unit.minute % 15;

    if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
        hours %= 12;
        if (hours == 0) hours = 12;
    }

    // static: the buzzer plays this back asynchronously over the next several
    // seconds, well after this function returns, so it can't live on the stack.
    // Max used: 6 (hour, 2 pairs + repeat marker) + 2 (quarter lead-in rest)
    // + 10 (quarter, 4 pairs + repeat marker) + 2 (minute lead-in rest)
    // + 6 (minute, 2 pairs + repeat marker) = 26, plus trailing zeros.
    static int8_t sound_seq[28];
    memset(sound_seq, 0, sizeof(sound_seq));
    int i = 0;

    if (hours > 0) {
        sound_seq[i++] = BUZZER_NOTE_C6;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(75);
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(500);
        if (hours > 1) {
            sound_seq[i++] = -2;
            sound_seq[i++] = (int8_t)(hours - 1);
        }
    }

    if (quarters > 0) {
        // a beat of silence to set the quarter chimes apart from the hour chimes.
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(200);
        sound_seq[i++] = BUZZER_NOTE_E6;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(75);
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(150);
        sound_seq[i++] = BUZZER_NOTE_C6;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(75);
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(750);
        if (quarters > 1) {
            sound_seq[i++] = -4;
            sound_seq[i++] = (int8_t)(quarters - 1);
        }
    }

    if (minutes > 0) {
        // a beat of silence to set the minute chimes apart from the quarter chimes.
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(200);
        sound_seq[i++] = BUZZER_NOTE_E6;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(75);
        sound_seq[i++] = BUZZER_NOTE_REST;
        sound_seq[i++] = MINUTE_REPEATER_TICKS(500);
        if (minutes > 1) {
            sound_seq[i++] = -2;
            sound_seq[i++] = (int8_t)(minutes - 1);
        }
    }

    watch_buzzer_play_sequence(sound_seq, NULL);
}

static void minute_repeater_indicate(watch_indicator_t indicator, bool on) {
    if (on) watch_set_indicator(indicator);
    else watch_clear_indicator(indicator);
}

static void minute_repeater_indicate_alarm(void) {
    minute_repeater_indicate(WATCH_INDICATOR_SIGNAL, movement_alarm_enabled());
}

static void minute_repeater_indicate_time_signal(minute_repeater_state_t *state) {
    minute_repeater_indicate(WATCH_INDICATOR_BELL, state->time_signal_enabled);
}

static void minute_repeater_indicate_24h(void) {
    minute_repeater_indicate(WATCH_INDICATOR_24H, !!movement_clock_mode_24h());
}

static bool minute_repeater_is_pm(watch_date_time_t date_time) {
    return date_time.unit.hour >= 12;
}

static void minute_repeater_indicate_pm(watch_date_time_t date_time) {
    if (movement_clock_mode_24h()) return;
    minute_repeater_indicate(WATCH_INDICATOR_PM, minute_repeater_is_pm(date_time));
}

static void minute_repeater_indicate_low_available_power(minute_repeater_state_t *state) {
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        minute_repeater_indicate(WATCH_INDICATOR_ARROWS, state->battery_low);
    } else {
        minute_repeater_indicate(WATCH_INDICATOR_LAP, state->battery_low);
    }
}

static watch_date_time_t minute_repeater_24h_to_12h(watch_date_time_t date_time) {
    date_time.unit.hour %= 12;
    if (date_time.unit.hour == 0) date_time.unit.hour = 12;
    return date_time;
}

static void minute_repeater_check_battery_periodically(minute_repeater_state_t *state, watch_date_time_t date_time) {
    if (date_time.unit.day == state->last_battery_check) return;

    state->last_battery_check = date_time.unit.day;
    state->battery_low = watch_get_vcc_voltage() < MINUTE_REPEATER_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD;
    minute_repeater_indicate_low_available_power(state);
}

static void minute_repeater_toggle_time_signal(minute_repeater_state_t *state) {
    state->time_signal_enabled = !state->time_signal_enabled;
    minute_repeater_indicate_time_signal(state);
}

static void minute_repeater_display_all(watch_date_time_t date_time) {
    char buf[8 + 1];

    snprintf(
        buf,
        sizeof(buf),
        movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ? "%02d%02d%02d%02d" : "%2d%2d%02d%02d",
        date_time.unit.day,
        date_time.unit.hour,
        date_time.unit.minute,
        date_time.unit.second
    );

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, watch_utility_get_long_weekday(date_time), watch_utility_get_weekday(date_time));
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    watch_display_text(WATCH_POSITION_BOTTOM, buf + 2);
}

static bool minute_repeater_display_some(watch_date_time_t current, watch_date_time_t previous) {
    if ((current.reg >> 6) == (previous.reg >> 6)) {
        // everything before seconds is the same, don't waste cycles setting those segments.
        watch_display_character_lp_seconds('0' + current.unit.second / 10, 8);
        watch_display_character_lp_seconds('0' + current.unit.second % 10, 9);
        return true;
    } else if ((current.reg >> 12) == (previous.reg >> 12)) {
        // everything before minutes is the same.
        char buf[4 + 1];
        snprintf(buf, sizeof(buf), "%02d%02d", current.unit.minute, current.unit.second);
        watch_display_text(WATCH_POSITION_MINUTES, buf);
        watch_display_text(WATCH_POSITION_SECONDS, buf + 2);
        return true;
    }

    return false;
}

static void minute_repeater_display_clock(minute_repeater_state_t *state, watch_date_time_t current) {
    if (!minute_repeater_display_some(current, state->previous)) {
        if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
            minute_repeater_indicate_pm(current);
            current = minute_repeater_24h_to_12h(current);
        }
        minute_repeater_display_all(current);
    }
}

static void minute_repeater_display_low_energy(watch_date_time_t date_time) {
    if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
        minute_repeater_indicate_pm(date_time);
        date_time = minute_repeater_24h_to_12h(date_time);
    }

    char buf[8 + 1];
    snprintf(
        buf,
        sizeof(buf),
        movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ? "%02d%02d%02d  " : "%2d%2d%02d  ",
        date_time.unit.day,
        date_time.unit.hour,
        date_time.unit.minute
    );

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, watch_utility_get_long_weekday(date_time), watch_utility_get_weekday(date_time));
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    watch_display_text(WATCH_POSITION_BOTTOM, buf + 2);
}

static void minute_repeater_start_tick_tock_animation(void) {
    if (!watch_sleep_animation_is_running()) {
        watch_start_sleep_animation(500);
        watch_start_indicator_blink_if_possible(WATCH_INDICATOR_COLON, 500);
    }
}

static void minute_repeater_stop_tick_tock_animation(void) {
    if (watch_sleep_animation_is_running()) {
        watch_stop_sleep_animation();
        watch_stop_blink();
    }
}

void minute_repeater_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(minute_repeater_state_t));
        minute_repeater_state_t *state = (minute_repeater_state_t *) *context_ptr;
        state->time_signal_enabled = false;
        state->watch_face_index = watch_face_index;
    }
}

void minute_repeater_face_activate(void *context) {
    minute_repeater_state_t *state = (minute_repeater_state_t *) context;

    minute_repeater_stop_tick_tock_animation();

    minute_repeater_indicate_time_signal(state);
    minute_repeater_indicate_alarm();
    minute_repeater_indicate_24h();

    watch_set_colon();

    // this ensures that none of the timestamp fields will match, so we can re-render them all.
    state->previous.reg = 0xFFFFFFFF;
}

bool minute_repeater_face_loop(movement_event_t event, void *context) {
    minute_repeater_state_t *state = (minute_repeater_state_t *) context;
    watch_date_time_t current;

    switch (event.event_type) {
        case EVENT_LOW_ENERGY_UPDATE:
            minute_repeater_start_tick_tock_animation();
            minute_repeater_display_low_energy(movement_get_local_date_time());
            break;
        case EVENT_TICK:
        case EVENT_ACTIVATE:
            current = movement_get_local_date_time();
            minute_repeater_display_clock(state, current);
            minute_repeater_check_battery_periodically(state, current);
            state->previous = current;
            break;
        case EVENT_ALARM_LONG_PRESS:
            minute_repeater_toggle_time_signal(state);
            break;
        case EVENT_LIGHT_LONG_UP:
            // The actual complication: chime out the current time.
            minute_repeater_chime_time(movement_get_local_date_time());
            break;
        case EVENT_BACKGROUND_TASK:
            // uncomment this line to snap back to this face when the hour signal sounds:
            // movement_move_to_face(state->watch_face_index);
            movement_play_signal();
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void minute_repeater_face_resign(void *context) {
    (void) context;
}

movement_watch_face_advisory_t minute_repeater_face_advise(void *context) {
    movement_watch_face_advisory_t retval = { 0 };
    minute_repeater_state_t *state = (minute_repeater_state_t *) context;

    if (state->time_signal_enabled) {
        watch_date_time_t date_time = movement_get_local_date_time();
        retval.wants_background_task = date_time.unit.minute == 0;
    }

    return retval;
}
