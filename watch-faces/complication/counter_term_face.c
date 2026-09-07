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
#include "counter_term_face.h"
#include "watch.h"
#include "watch_utility.h"

// Julian day number for a Gregorian calendar date -- used to difference two dates by whole
// calendar days regardless of month/year boundaries, without a full unix-time round trip.
// https://en.wikipedia.org/wiki/Julian_day#Julian_day_number_calculation (same formula
// days_since_face.c uses).
static uint32_t _counter_term_face_juliandaynum(uint16_t year, uint16_t month, uint16_t day) {
    return (1461 * (year + 4800 + (month - 14) / 12)) / 4 + (367 * (month - 2 - 12 * ((month - 14) / 12))) / 12 - (3 * ((year + 4900 + (month - 14) / 12) / 100)) / 4 + day - 32075;
}

static void _counter_term_face_record_today(counter_term_state_t *state) {
    watch_date_time_t date_time = movement_get_local_date_time();
    state->start_year = date_time.unit.year + WATCH_RTC_REFERENCE_YEAR;
    state->start_month = date_time.unit.month;
    state->start_day = date_time.unit.day;
}

static void _counter_term_face_update(counter_term_state_t *state) {
    // 6 bytes: large enough for "%4d" of a uint16_t counter (up to 5 digits + nul) even though
    // EVENT_ALARM_BUTTON_UP's `% 10000` keeps the runtime value to 4 digits -- the type itself
    // doesn't carry that bound, so gcc sizes its -Wformat-overflow check against uint16_t's full
    // range (up to 65535) rather than the actual guarantee.
    char buf[6];

    // Days elapsed since the recorded start date. Custom uses the full 5-character TOP;
    // classic uses TOP_RIGHT (2 characters) and wraps at 40 rather than 100, since its
    // tens digit only renders 0-3.
    watch_date_time_t date_time = movement_get_local_date_time();
    uint32_t julian_now = _counter_term_face_juliandaynum(date_time.unit.year + WATCH_RTC_REFERENCE_YEAR, date_time.unit.month, date_time.unit.day);
    uint32_t julian_start = _counter_term_face_juliandaynum(state->start_year, state->start_month, state->start_day);
    uint32_t elapsed = (julian_now > julian_start) ? (julian_now - julian_start) : 0;
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        elapsed %= 100000;
        sprintf(buf, "%5u", (unsigned int) elapsed);
        watch_display_text_with_fallback(WATCH_POSITION_TOP, buf, buf);
    } else {
        elapsed %= 40;
        sprintf(buf, "%2u", (unsigned int) elapsed);
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    }

    // Counter occupies hours+minutes (4 digits). Seconds and colon are cleared since this
    // isn't a time display.
    sprintf(buf, "%4d", state->counter);
    watch_display_text(WATCH_POSITION_HOURS, buf);
    watch_display_text(WATCH_POSITION_MINUTES, buf + 2);
    watch_display_text(WATCH_POSITION_SECONDS, "  ");
    watch_clear_colon();
}

void counter_term_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(counter_term_state_t));
        memset(*context_ptr, 0, sizeof(counter_term_state_t));
        counter_term_state_t *state = (counter_term_state_t *)*context_ptr;
        _counter_term_face_record_today(state);
    }
}

void counter_term_face_activate(void *context) {
    (void) context;
}

bool counter_term_face_loop(movement_event_t event, void *context) {
    counter_term_state_t *state = (counter_term_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _counter_term_face_update(state);
            break;
        case EVENT_ALARM_BUTTON_UP:
            state->counter = (state->counter + 1) % 10000;
            _counter_term_face_update(state);
            if (movement_button_should_sound()) watch_buzzer_play_note_with_volume(BUZZER_NOTE_C7, 50, movement_button_volume());
            break;
        case EVENT_ALARM_LONG_PRESS:
            state->counter = 0;
            _counter_term_face_record_today(state);
            _counter_term_face_update(state);
            if (movement_button_should_sound()) watch_buzzer_play_note_with_volume(BUZZER_NOTE_C8, 50, movement_button_volume());
            break;
        case EVENT_TICK: {
            // The elapsed-days count only ever needs to change at local midnight.
            watch_date_time_t date_time = movement_get_local_date_time();
            if (date_time.unit.hour == 0 && date_time.unit.minute == 0 && date_time.unit.second == 0) {
                _counter_term_face_update(state);
            }
            break;
        }
        case EVENT_LOW_ENERGY_UPDATE:
            // Keeps the elapsed-days count accurate across a midnight passed while asleep.
            _counter_term_face_update(state);
            break;
        case EVENT_TIMEOUT:
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void counter_term_face_resign(void *context) {
    (void) context;
}
