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

// 先勝,友引,先負,仏滅,大安,赤口, 2 characters each at TOP_LEFT (positions 0/1) on classic.
// Position 0 is fully independent (all 8 segments), but position 1 aliases 1B/1C and 1E/1F
// to single addresses (see Classic_LCD_Display_Mapping), so a position-1 character only
// renders correctly if its font byte's B==C and E==F. An earlier revision used 'S' as 先勝's
// second character ("SS"), which doesn't satisfy that (B=0/C=1) -- replaced with 'E' here.
// All 6 second characters (E/B/B/t/A/H) verified B==C and E==F at position 1.
static const char *rokuyo_names_classic[6] = {
    "SE", "TB", "SB", "Bt", "TA", "SH",
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

// Shifts a date by delta_days via a unix-time round trip (handles month/year rollover for
// free), zeroing time-of-day first so the round trip can't land on the wrong calendar day
// from a DST-like offset shift -- same approach as wadokei_face's own _wadokei_shift_day.
static watch_date_time_t _kyureki_shift_day(watch_date_time_t day, int32_t delta_days) {
    day.unit.hour = 0;
    day.unit.minute = 0;
    day.unit.second = 0;
    uint32_t timestamp = watch_utility_date_time_to_unix_time(day, 0);
    timestamp = (uint32_t)((int64_t)timestamp + (int64_t)delta_days * 86400);
    return watch_utility_date_time_from_unix_time(timestamp, 0);
}

static void _kyureki_face_update(kyureki_state_t *state) {
    watch_date_time_t now = movement_get_local_date_time();
    if (state->offset_days) now = _kyureki_shift_day(now, state->offset_days);

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

    // Month m's 1st day is at sequence position (m-1)%6 (month 1 and 7 both start the
    // cycle at 先勝/index 0, month 2 and 8 at 友引/index 1, etc.), and day d of that month
    // is (d-1) further along -- combined, (m-1)+(d-1) = m+d-2. Plain (m+d)%6 (no "-2") is
    // off by a constant +2 for every date: e.g. lunar 7/19 (index 0 -> 先勝, confirmed
    // against an independent lunar calendar reference) came out as (7+19)%6=2 -> 先負
    // instead. month_number/day_of_month are always >=1, so m+d-2 never goes negative.
    uint8_t rokuyo_index = (state->month_number + state->day_of_month - 2) % 6;
    // watch_display_text_with_fallback()'s TOP case only reaches all 5 characters on custom;
    // its classic fallback path falls through to plain watch_display_text(), which only ever
    // writes the *first 2* of whatever string it's given -- so passing rokuyo_names (the
    // 5-char custom spelling) as its own fallback would silently show the wrong abbreviation
    // on classic (e.g. 友引's "TOMBK" truncates to "TO", not the "TB" rokuyo_names_classic
    // actually calls for). rokuyo_names_classic is the deliberately-chosen 2-character form
    // instead (see its own comment for the position-1 aliasing it was checked against).
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *)rokuyo_names[rokuyo_index], (char *)rokuyo_names[rokuyo_index]);
    } else {
        watch_display_text(WATCH_POSITION_TOP_LEFT, (char *)rokuyo_names_classic[rokuyo_index]);
        // TOP_RIGHT is otherwise unused by this face on classic -- while browsing
        // (offset_days != 0), it shows the *solar* (Gregorian) day-of-month of the date being
        // browsed to, not the offset count itself -- same idea as custom's own SECONDS tail
        // below; blanked again once back on today. Always 1-31, so the tens digit is only
        // ever blank, 1, 2, or 3 -- never one of the 0/4/7 values position 2's A=D=G triple
        // alias (see Classic_LCD_Display_Mapping) can't render correctly, unlike the raw
        // offset count this replaced (which had no such bound).
        char offset_buf[3];
        if (state->offset_days != 0) {
            snprintf(offset_buf, sizeof(offset_buf), "%2d", now.unit.day);
        } else {
            snprintf(offset_buf, sizeof(offset_buf), "  ");
        }
        watch_display_text(WATCH_POSITION_TOP_RIGHT, offset_buf);
    }

    // "month.day", decimal point lit between them. Last 2 chars (SECONDS) are normally "Ud"
    // for a leap month, blank otherwise -- but on custom, while browsing (offset_days != 0),
    // they show the *solar* (Gregorian) day-of-month of the date being browsed to instead
    // (variable-width, e.g. " 6"/"31"), so the wearer can see what calendar date the browsed
    // lunar date actually falls on. This is custom-only (classic's own TOP_RIGHT carries the
    // same thing -- see above).
    char buf[7];
    char tail[3];
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM && state->offset_days != 0) {
        snprintf(tail, sizeof(tail), "%2d", now.unit.day);
    } else {
        snprintf(tail, sizeof(tail), "%s", state->is_leap ? "Ud" : "  ");
    }
    snprintf(buf, sizeof(buf), "%2d%2d%s", state->month_number, state->day_of_month, tail);
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
            _kyureki_face_update(state);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // Matches moon_phase_ascii_face's own day-offset browsing: kill the offset here
            // too, so a wearer who falls asleep mid-browse wakes up back on today rather than
            // wherever they'd stepped to.
            state->offset_days = 0;
            _kyureki_face_update(state);
            break;
        case EVENT_ALARM_BUTTON_UP:
            state->offset_days++;
            _kyureki_face_update(state);
            break;
        case EVENT_ALARM_LONG_PRESS:
            state->offset_days = 0;
            _kyureki_face_update(state);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            // Swallow this (rather than falling through to the default handler) so the
            // ordinary "tap Light to illuminate" behavior doesn't fire alongside the
            // step-back-a-day action below -- same as moon_phase_ascii_face's own Light
            // handling.
            break;
        case EVENT_LIGHT_BUTTON_UP:
            state->offset_days--;
            _kyureki_face_update(state);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void kyureki_face_resign(void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;
    state->offset_days = 0;
    watch_clear_decimal_if_available();
}
