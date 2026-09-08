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
#include <math.h>
#include "kyurekisekki_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "kyurekisekki_table_data.h"
#include "kyurekisekki_names_data.h"
#include "sunriset.h"

// 六曜: 先勝,友引,先負,仏滅,大安,赤口, for WATCH_POSITION_TOP (custom LCD).
static const char *rokuyo_names[6] = {
    "SENSY", "TOMBK", "SEMBU", "BTMTS", "TAIAN", "SHAKK",
};

// 六曜 abbreviations for TOP_LEFT on classic.
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
        // Still in the tail of the previous lunar year.
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
        // Defensive fallback for a malformed table entry.
        month_number = 12;
        is_leap = false;
        remaining = 0;
    }

    state->month_number = month_number;
    state->is_leap = is_leap;
    state->day_of_month = remaining + 1;
    state->valid = true;
}

// Shifts a date by delta_days via a unix-time round trip, zeroing time-of-day first to avoid landing on the wrong calendar day.
static watch_date_time_t _kyureki_shift_day(watch_date_time_t day, int32_t delta_days) {
    day.unit.hour = 0;
    day.unit.minute = 0;
    day.unit.second = 0;
    uint32_t timestamp = watch_utility_date_time_to_unix_time(day, 0);
    timestamp = (uint32_t)((int64_t)timestamp + (int64_t)delta_days * 86400);
    return watch_utility_date_time_from_unix_time(timestamp, 0);
}

// ecliptic longitude (degrees) of term index 0, 立春; each subsequent term is +15 degrees.
#define SEKKI_BASE_LONGITUDE 315.0
#define SEKKI_COUNT 24
// mean rate of the Sun's apparent motion along the ecliptic, degrees/day (365.2422-day year).
#define SEKKI_DEG_PER_DAY (360.0 / 365.2422)

static double _sekki_solar_longitude_unix(uint32_t timestamp) {
    watch_date_time_t dt = watch_utility_date_time_from_unix_time(timestamp, 0);
    double hour = dt.unit.hour + dt.unit.minute / 60.0 + dt.unit.second / 3600.0;
    return sun_ecliptic_longitude(dt.unit.year + WATCH_RTC_REFERENCE_YEAR, dt.unit.month, dt.unit.day, hour);
}

// Newton's method for the unix time nearest guess_unix at which the Sun's ecliptic longitude equals target_lon (0-360 degrees).
static uint32_t _sekki_solve_unix(double target_lon, int64_t guess_unix) {
    int64_t t = guess_unix;
    for (uint8_t iter = 0; iter < 8; iter++) {
        double diff = target_lon - _sekki_solar_longitude_unix((uint32_t)t);
        while (diff > 180.0) diff -= 360.0;
        while (diff <= -180.0) diff += 360.0;
        if (fabs(diff) < 1e-4) break;
        t += (int64_t)llround((diff / SEKKI_DEG_PER_DAY) * 86400.0);
    }
    return (uint32_t)t;
}

// Determines the term at `offset_terms` from the one nearest to today, and stores its index and Gregorian date into state.
static void _sekki_compute(kyureki_state_t *state, watch_date_time_t now, int32_t offset_terms) {
    // A solar term's crossing instant matters down to the hour, so this needs the wearer's real UTC offset, not 0.
    int32_t utc_offset = movement_get_current_timezone_offset();
    uint32_t now_unix = watch_utility_date_time_to_unix_time(now, utc_offset);
    double now_lon = _sekki_solar_longitude_unix(now_unix);

    double phase = now_lon - SEKKI_BASE_LONGITUDE; // degrees past term 0, in [0, 360)
    while (phase < 0.0) phase += 360.0;
    while (phase >= 360.0) phase -= 360.0;

    int32_t prev_index = (int32_t)(phase / 15.0); // last term at/before `now`
    int32_t next_index = prev_index + 1;           // first term after `now`

    // Solve both neighbors' actual dates and compare by real elapsed time, since the Sun's speed along the ecliptic isn't quite constant.
    double prev_lon = SEKKI_BASE_LONGITUDE + prev_index * 15.0;
    while (prev_lon >= 360.0) prev_lon -= 360.0;
    double next_lon = SEKKI_BASE_LONGITUDE + next_index * 15.0;
    while (next_lon >= 360.0) next_lon -= 360.0;

    int64_t prev_guess = (int64_t)now_unix - (int64_t)llround((phase - prev_index * 15.0) / SEKKI_DEG_PER_DAY * 86400.0);
    int64_t next_guess = (int64_t)now_unix + (int64_t)llround((next_index * 15.0 - phase) / SEKKI_DEG_PER_DAY * 86400.0);
    uint32_t prev_unix = _sekki_solve_unix(prev_lon, prev_guess);
    uint32_t next_unix = _sekki_solve_unix(next_lon, next_guess);

    int32_t nearest_index = (llabs((int64_t)now_unix - (int64_t)prev_unix) <= llabs((int64_t)next_unix - (int64_t)now_unix))
        ? prev_index : next_index;

    int32_t index = nearest_index + offset_terms;
    int32_t norm_index = ((index % SEKKI_COUNT) + SEKKI_COUNT) % SEKKI_COUNT;

    uint32_t term_unix;
    if (index == prev_index) {
        term_unix = prev_unix;
    } else if (index == next_index) {
        term_unix = next_unix;
    } else {
        // Browsed further out than the two neighbors already solved -- extrapolate a fresh guess for the Newton solve.
        double target_lon = SEKKI_BASE_LONGITUDE + norm_index * 15.0;
        if (target_lon >= 360.0) target_lon -= 360.0;
        double terms_ahead = (double)index - (phase / 15.0);
        int64_t guess_unix = (int64_t)now_unix + (int64_t)llround(terms_ahead * (365.2422 / SEKKI_COUNT) * 86400.0);
        term_unix = _sekki_solve_unix(target_lon, guess_unix);
    }

    state->sekki_term_date = watch_utility_date_time_from_unix_time(term_unix, utc_offset);
    state->sekki_term_index = (uint8_t)norm_index;
}

static void _sekki_face_update(kyureki_state_t *state) {
    watch_date_time_t now = movement_get_local_date_time();

    if (state->sekki_computed_date.unit.year != now.unit.year ||
        state->sekki_computed_date.unit.month != now.unit.month ||
        state->sekki_computed_date.unit.day != now.unit.day ||
        state->sekki_computed_offset != state->sekki_offset) {
        _sekki_compute(state, now, state->sekki_offset);
        state->sekki_computed_date = now;
        state->sekki_computed_offset = state->sekki_offset;
    }

    uint16_t full_year = state->sekki_term_date.unit.year + WATCH_RTC_REFERENCE_YEAR;
    char date_buf[7];
    snprintf(date_buf, sizeof(date_buf), "%2d%2d%02d", state->sekki_term_date.unit.month,
              state->sekki_term_date.unit.day, full_year % 100);

    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        // The fallback variant is what actually drives all 5 custom-LCD TOP digits.
        watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *)sekki_names_5[state->sekki_term_index], (char *)sekki_names_5[state->sekki_term_index]);
        watch_set_decimal_if_available();
        watch_display_text(WATCH_POSITION_BOTTOM, date_buf);
    } else {
        // Blank TOP_LEFT/TOP_RIGHT in case the lunar/rokuyo display left something behind there.
        watch_display_text(WATCH_POSITION_TOP_LEFT, "  ");
        watch_display_text(WATCH_POSITION_TOP_RIGHT, "  ");
        if ((now.unit.second / 2) % 2 == 0) {
            watch_display_text(WATCH_POSITION_BOTTOM, (char *)sekki_names_6[state->sekki_term_index]);
        } else {
            watch_display_text(WATCH_POSITION_BOTTOM, date_buf);
        }
    }
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

    // (m-1)+(d-1) = m+d-2 is the sequence position; month_number/day_of_month are always >=1, so it never goes negative.
    uint8_t rokuyo_index = (state->month_number + state->day_of_month - 2) % 6;
    // Classic's fallback path truncates to 2 characters, so it needs rokuyo_names_classic's own abbreviation.
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *)rokuyo_names[rokuyo_index], (char *)rokuyo_names[rokuyo_index]);
    } else {
        watch_display_text(WATCH_POSITION_TOP_LEFT, (char *)rokuyo_names_classic[rokuyo_index]);
        // While browsing, shows the solar day-of-month rather than the raw offset count.
        char offset_buf[3];
        if (state->offset_days != 0) {
            snprintf(offset_buf, sizeof(offset_buf), "%2d", now.unit.day);
        } else {
            snprintf(offset_buf, sizeof(offset_buf), "  ");
        }
        watch_display_text(WATCH_POSITION_TOP_RIGHT, offset_buf);
    }

    // "month.day"; last 2 chars are "Ud" for a leap month, blank otherwise, or the solar day-of-month while browsing on custom.
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

static void _face_update(kyureki_state_t *state) {
    if (state->sekki_mode) {
        _sekki_face_update(state);
    } else {
        _kyureki_face_update(state);
    }
}

void kyurekisekki_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(kyureki_state_t));
        memset(*context_ptr, 0, sizeof(kyureki_state_t));
    }
}

void kyurekisekki_face_activate(void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;
    // force recompute on activation
    state->last_computed_date.reg = 0xFFFFFFFF;
    state->sekki_computed_date.reg = 0xFFFFFFFF;
    _face_update(state);
}

bool kyurekisekki_face_loop(movement_event_t event, void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_TICK:
            _face_update(state);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // Kill the offset so a wearer who falls asleep mid-browse wakes up back on today.
            state->offset_days = 0;
            state->sekki_offset = 0;
            _face_update(state);
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->sekki_mode) {
                state->sekki_offset++;
            } else {
                state->offset_days++;
            }
            _face_update(state);
            break;
        case EVENT_ALARM_LONG_PRESS:
            // Toggle modes, resetting both modes' browsing offsets so each starts fresh.
            state->sekki_mode = !state->sekki_mode;
            state->offset_days = 0;
            state->sekki_offset = 0;
            _face_update(state);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            // Swallow so the ordinary "tap Light to illuminate" doesn't fire alongside the step-back below.
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->sekki_mode) {
                state->sekki_offset--;
            } else {
                state->offset_days--;
            }
            _face_update(state);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void kyurekisekki_face_resign(void *context) {
    kyureki_state_t *state = (kyureki_state_t *)context;
    state->offset_days = 0;
    state->sekki_mode = false;
    state->sekki_offset = 0;
    watch_clear_decimal_if_available();
}
