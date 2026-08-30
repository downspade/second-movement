/*
 * MIT License
 *
 * Copyright (c) 2026 zippie
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
#include <stdio.h>
#include "kyureki_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "kyureki_table_data.h"

// 先勝,友引,先負,仏滅,大安,赤口, 5 characters each for WATCH_POSITION_TOP (custom LCD).
// Uppercase 'I' renders incorrectly outside display position 0 on the custom LCD (see
// Custom_LCD_Character_Set in watch_common_display.h), hence the lowercase 'i' in
// TMBKi/TAiAN, matching the workaround used elsewhere (e.g. probability_face.c's "TAiLS").
// 'M' and 'T' outside position 0 have a similar minor rendering quirk that the existing
// codebase accepts as-is elsewhere (e.g. periodic_table_face.c's "Table"), so those are
// left uppercase here too.
static const char *rokuyo_names[6] = {
    "SENSY", "TOMBK", "SEMBU", "BTMTS", "TAIAN", "SHAKK",
};

static uint16_t _kyureki_day_of_year(uint8_t month, uint8_t day, uint16_t year) {
    uint16_t doy = 0;
    for (uint8_t m = 1; m < month; m++) doy += watch_utility_days_in_month(m, year);
    doy += (day - 1);
    return doy;
}

static uint16_t _kyureki_days_in_year(uint16_t year) {
    uint16_t total = 0;
    for (uint8_t m = 1; m <= 12; m++) total += watch_utility_days_in_month(m, year);
    return total;
}

static void _kyureki_compute(kyureki_state_t *state, watch_date_time_t date) {
    state->last_computed_date = date;

    uint16_t year = date.unit.year + WATCH_RTC_REFERENCE_YEAR;
    if (year < KYUREKI_TABLE_BASE_YEAR || year > KYUREKI_TABLE_BASE_YEAR + KYUREKI_TABLE_NUM_YEARS - 1) {
        state->valid = false;
        return;
    }

    uint16_t doy = _kyureki_day_of_year(date.unit.month, date.unit.day, year);
    const lunar_year_t *this_year_entry = &kyureki_table[year - KYUREKI_TABLE_BASE_YEAR];

    const lunar_year_t *year_data;
    uint16_t days_since_new_year;

    if (doy >= this_year_entry->days_to_new_year) {
        year_data = this_year_entry;
        days_since_new_year = doy - this_year_entry->days_to_new_year;
    } else {
        // Still in the tail of the previous lunar year (before this Gregorian year's New Year).
        if (year - 1 < KYUREKI_TABLE_BASE_YEAR) {
            state->valid = false;
            return;
        }
        const lunar_year_t *prev_year_entry = &kyureki_table[year - 1 - KYUREKI_TABLE_BASE_YEAR];
        uint16_t days_in_prev_year = _kyureki_days_in_year(year - 1);
        year_data = prev_year_entry;
        days_since_new_year = (days_in_prev_year - prev_year_entry->days_to_new_year) + doy;
    }

    uint8_t month_number = 1;
    bool is_leap = false;
    uint16_t remaining = days_since_new_year;
    uint8_t i;
    for (i = 0; i < 13; i++) {
        uint8_t month_len = (year_data->month_lengths & (1 << i)) ? 30 : 29;
        if (remaining < month_len) break;
        remaining -= month_len;
        if (month_number == year_data->leap_month && !is_leap) {
            is_leap = true;
        } else {
            month_number++;
            is_leap = false;
        }
    }
    if (i == 13) {
        // Defensive fallback: a malformed table entry shouldn't be able to produce an
        // out-of-range display.
        month_number = 12;
        is_leap = false;
        remaining = 0;
    }

    state->month_number = month_number;
    state->is_leap = is_leap;
    state->day_of_month = remaining + 1;
    state->valid = true;
}

static void _kyureki_face_update(kyureki_state_t *state) {
    watch_date_time_t now = movement_get_local_date_time();

    if (state->last_computed_date.unit.year != now.unit.year ||
        state->last_computed_date.unit.month != now.unit.month ||
        state->last_computed_date.unit.day != now.unit.day) {
        _kyureki_compute(state, now);
    }

    if (!state->valid) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, "ERR", "Er");
        watch_clear_decimal_if_available();
        watch_display_text(WATCH_POSITION_BOTTOM, "None  ");
        return;
    }

    uint8_t rokuyo_index = (state->month_number + state->day_of_month) % 6;
    watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *)rokuyo_names[rokuyo_index], (char *)rokuyo_names[rokuyo_index]);

    // "month.day", decimal point lit between them; last 2 chars are "Ud" for a leap month.
    char buf[7];
    snprintf(buf, sizeof(buf), "%2d%02d%s", state->month_number, state->day_of_month, state->is_leap ? "Ud" : "  ");
    watch_set_decimal_if_available();
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

void kyureki_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(kyureki_state_t));
        memset(*context_ptr, 0, sizeof(kyureki_state_t));
    }
}

void kyureki_face_activate(void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;
    // force recompute on activation
    state->last_computed_date.reg = 0xFFFFFFFF;
    _kyureki_face_update(state);
}

bool kyureki_face_loop(movement_event_t event, void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            _kyureki_face_update(state);
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void kyureki_face_resign(void *context) {
    (void) context;
    watch_clear_decimal_if_available();
}
