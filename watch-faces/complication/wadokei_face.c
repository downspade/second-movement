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
#include <math.h>
#include "wadokei_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "filesystem.h"
#include "sunriset.h"

// Rika Nenpyo (理科年表) definition of 明六つ/暮六つ: the sun's center is
// 7 degrees 21 minutes 40 seconds below the horizon, rather than the usual
// -6 degree civil twilight.
#define WADOKEI_TWILIGHT_ALTITUDE (-7.361111)

// The 12 branches, indexed 0=子,1=丑,2=寅,3=卯,4=辰,5=巳,6=午,7=未,8=申,9=酉,10=戌,11=亥
// (matches the row order in koku.csv), 5 characters each for WATCH_POSITION_TOP (custom
// LCD). Note: uppercase 'I' renders incorrectly outside display position 0 on the custom
// LCD (see Custom_LCD_Character_Set in watch_common_display.h); substituting lowercase
// 'i' here follows the same workaround already used elsewhere (e.g. probability_face.c's
// "TAiLS", blackjack_face.c's "WlN"/"TlE"). 'M' outside position 0 has a similar minor
// quirk that the existing codebase accepts as-is (e.g. "Table"), so UMA is left uppercase.
static const char *branch_names[12] = {
    "NE   ", "USHi ", "TORA ", "U    ", "TATSU", "Mi   ",
    "UMA  ", "HiTJi", "SARU ", "TORi ", "iNU  ", "i    ",
};

// One quarter name per position, spelled out in full (一つ/二つ/三つ/四つ), 6 characters
// each for WATCH_POSITION_BOTTOM.
static const char *quarter_names[4] = {
    "HiTOTU", "FUTATU", "MiTTSU", "YOTTSU",
};

// Traditional bell-count ("koku") names, per koku.csv: each branch has a bell-count digit
// (cycling 9,8,7,6,5,4 twice per day) and a time-of-day prefix. Quarters 1-2 (一つ/二つ)
// use the plain name; quarters 3-4 (三つ/四つ) append "HAN" (半).
// "Hi" (昼) would collide with position-0-only 'I' outside position 0, so it's "HiR".
static const char *koku_prefix[12] = {
    "AKA", "AKA", "AKA", "AKE", "ASA", "ASA",
    "HiR", "HiR", "HiR", "KUR", "YOR", "YOR",
};
static const uint8_t koku_digit[12] = { 9, 8, 7, 6, 5, 4, 9, 8, 7, 6, 5, 4 };

// Per koku.csv: only 卯 and 酉 split their four quarters across two different 24ths-of-a-
// span lengths (卯's 一つ二つ belong to the *previous* night's 24-division; 酉's 三つ四つ
// belong to the *next* night's). Every other branch's 4 quarters are uniform, all drawn
// from the current day-span (rough branches) or the current night-span (夜 branches).
// These tables map a 0-23 "24th of the current span" index to (branch, quarter).
static const uint8_t day_piece_branch[24] = {
    3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9,
};
static const uint8_t day_piece_quarter[24] = {
    2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1,
};
static const uint8_t night_piece_branch[24] = {
    9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
};
static const uint8_t night_piece_quarter[24] = {
    2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1,
};

static watch_date_time_t _wadokei_day_midnight(watch_date_time_t day) {
    day.unit.hour = 0;
    day.unit.minute = 0;
    day.unit.second = 0;
    return day;
}

static watch_date_time_t _wadokei_shift_day(watch_date_time_t day, int32_t delta_days) {
    watch_date_time_t midnight = _wadokei_day_midnight(day);
    uint32_t timestamp = watch_utility_date_time_to_unix_time(midnight, 0);
    timestamp = (uint32_t)((int64_t)timestamp + (int64_t)delta_days * 86400);
    return watch_utility_date_time_from_unix_time(timestamp, 0);
}

// Computes dawn-twilight-start and dusk-twilight-end for the given calendar day, as
// unix timestamps (consistent with the utc_offset=0 arithmetic scheme used throughout
// this face). Returns false if __sunriset__ can't resolve a crossing that day (polar
// day/night at extreme latitudes).
static bool _wadokei_twilight_bounds_unix(watch_date_time_t day, movement_location_t location,
                                           uint32_t *dawn_start_unix, uint32_t *dusk_end_unix) {
    int16_t lat_centi = (int16_t)location.bit.latitude;
    int16_t lon_centi = (int16_t)location.bit.longitude;
    double lat = (double)lat_centi / 100.0;
    double lon = (double)lon_centi / 100.0;

    double rise, set;
    int result = __sunriset__(day.unit.year + WATCH_RTC_REFERENCE_YEAR, day.unit.month, day.unit.day,
                               lon, lat, WADOKEI_TWILIGHT_ALTITUDE, 0, &rise, &set);
    if (result != 0) return false;

    double hours_from_utc = ((double)movement_get_timezone_offset_for_date(day)) / 3600.0;
    rise += hours_from_utc;
    set += hours_from_utc;

    uint32_t midnight_unix = watch_utility_date_time_to_unix_time(_wadokei_day_midnight(day), 0);
    *dawn_start_unix = (uint32_t)((int64_t)midnight_unix + (int64_t)llround(rise * 3600.0));
    *dusk_end_unix = (uint32_t)((int64_t)midnight_unix + (int64_t)llround(set * 3600.0));
    return true;
}

// Determines which branch span (a day-side or night-side twilight-to-twilight range)
// `now` falls into, and caches it in state. May need yesterday's or tomorrow's twilight
// bounds when `now` falls outside today's daytime span (see plan: 3 cases + failure).
static void _wadokei_compute_span(wadokei_state_t *state, movement_location_t location,
                                   uint32_t now_unix, watch_date_time_t today) {
    uint32_t today_dawn, today_dusk;
    if (!_wadokei_twilight_bounds_unix(today, location, &today_dawn, &today_dusk)) {
        state->valid = false;
        return;
    }

    if (now_unix < today_dawn) {
        // Still last night: yesterday's dusk twilight through today's dawn twilight.
        watch_date_time_t yesterday = _wadokei_shift_day(today, -1);
        uint32_t y_dawn, y_dusk;
        if (!_wadokei_twilight_bounds_unix(yesterday, location, &y_dawn, &y_dusk)) {
            state->valid = false;
            return;
        }
        state->span_start_unix = y_dusk;
        state->span_end_unix = today_dawn;
        state->is_daytime = false;
    } else if (now_unix < today_dusk) {
        state->span_start_unix = today_dawn;
        state->span_end_unix = today_dusk;
        state->is_daytime = true;
    } else {
        watch_date_time_t tomorrow = _wadokei_shift_day(today, 1);
        uint32_t t_dawn, t_dusk;
        if (!_wadokei_twilight_bounds_unix(tomorrow, location, &t_dawn, &t_dusk)) {
            state->valid = false;
            return;
        }
        state->span_start_unix = today_dusk;
        state->span_end_unix = t_dawn;
        state->is_daytime = false;
    }
    state->valid = true;
}

static int16_t _wadokei_latlon_from_struct(wadokei_lat_lon_settings_t val) {
    int16_t retval = (val.sign ? -1 : 1) *
                        (
                            val.hundreds * 10000 +
                            val.tens * 1000 +
                            val.ones * 100 +
                            val.tenths * 10 +
                            val.hundredths
                        );
    return retval;
}

static wadokei_lat_lon_settings_t _wadokei_struct_from_latlon(int16_t val) {
    wadokei_lat_lon_settings_t retval;
    retval.sign = val < 0;
    val = abs(val);
    retval.hundredths = val % 10;
    val /= 10;
    retval.tenths = val % 10;
    val /= 10;
    retval.ones = val % 10;
    val /= 10;
    retval.tens = val % 10;
    val /= 10;
    retval.hundreds = val % 10;
    return retval;
}

static void _wadokei_persist_location(movement_location_t new_location) {
    movement_location_t maybe_location = {0};
    filesystem_read_file("location.u32", (char *) &maybe_location.reg, sizeof(movement_location_t));
    if (new_location.reg != maybe_location.reg) {
        filesystem_write_file("location.u32", (char *) &new_location.reg, sizeof(movement_location_t));
    }
}

static void _wadokei_update_location_register(wadokei_state_t *state) {
    if (state->location_changed) {
        movement_location_t movement_location;
        movement_location.bit.latitude = _wadokei_latlon_from_struct(state->working_latitude);
        movement_location.bit.longitude = _wadokei_latlon_from_struct(state->working_longitude);
        _wadokei_persist_location(movement_location);
        state->location_changed = false;
        state->valid = false; // force a fresh span recompute with the new location
    }
}

// Custom-LCD-only digit editor for latitude/longitude, copied (and trimmed of the named-
// preset cycling this face doesn't use) from sunrise_sunset_face.c's settings pages.
static void _wadokei_update_settings_display(movement_event_t event, wadokei_state_t *state) {
    watch_clear_display();

    switch (state->page) {
        case 0:
            return;
        case 1: // latitude
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LAT", "L");
            watch_set_decimal_if_available();
            watch_display_character('0' + state->working_latitude.tens, 4);
            watch_display_character('0' + state->working_latitude.ones, 5);
            watch_display_character('0' + state->working_latitude.tenths, 6);
            watch_display_character('0' + state->working_latitude.hundredths, 7);
            watch_display_character('#', 8);
            watch_display_character(state->working_latitude.sign ? 'S' : 'N', 9);
            if (event.subsecond % 2) {
                watch_display_character(' ', 4 + state->active_digit);
                if (state->active_digit == 4) watch_display_character(' ', 9);
            }
            break;
        case 2: // longitude
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LON", "L");
            watch_set_decimal_if_available();
            if (state->working_longitude.hundreds == 1) watch_set_pixel(0, 22);
            watch_display_character('0' + state->working_longitude.tens, 4);
            watch_display_character('0' + state->working_longitude.ones, 5);
            watch_display_character('0' + state->working_longitude.tenths, 6);
            watch_display_character('0' + state->working_longitude.hundredths, 7);
            watch_display_character('#', 8);
            watch_display_character(state->working_longitude.sign ? 'W' : 'E', 9);
            if (event.subsecond % 2) {
                watch_display_character(' ', 4 + state->active_digit);
                if (state->active_digit == 0) watch_clear_pixel(0, 22);
                if (state->active_digit == 4) watch_display_character(' ', 9);
            }
            break;
    }
}

static void _wadokei_advance_digit(wadokei_state_t *state) {
    state->location_changed = true;
    switch (state->page) {
        case 1: // latitude
            switch (state->active_digit) {
                case 0:
                    state->working_latitude.tens = (state->working_latitude.tens + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_latitude)) > 9000) {
                        state->working_latitude.ones = 0;
                        state->working_latitude.tenths = 0;
                        state->working_latitude.hundredths = 0;
                    }
                    break;
                case 1:
                    state->working_latitude.ones = (state->working_latitude.ones + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                    break;
                case 2:
                    state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                    break;
                case 3:
                    state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
                    break;
                case 4:
                    state->working_latitude.sign++;
                    break;
            }
            break;
        case 2: // longitude
            switch (state->active_digit) {
                case 0:
                    state->working_longitude.tens++;
                    if (state->working_longitude.tens >= 10) {
                        state->working_longitude.tens = 0;
                        state->working_longitude.hundreds++;
                    }
                    if (abs(_wadokei_latlon_from_struct(state->working_longitude)) > 18000) {
                        state->working_longitude.hundreds = 0;
                        state->working_longitude.tens = 0;
                        state->working_longitude.ones = 0;
                        state->working_longitude.tenths = 0;
                        state->working_longitude.hundredths = 0;
                    }
                    break;
                case 1:
                    state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                    break;
                case 2:
                    state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                    break;
                case 3:
                    state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                    if (abs(_wadokei_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                    break;
                case 4:
                    state->working_longitude.sign++;
                    break;
            }
            break;
    }
}

static void _wadokei_face_update(wadokei_state_t *state) {
    // The lat/lon settings screen lights this fixed decimal-point pixel; make sure it's
    // off whenever we're back on the normal (non-settings) display.
    watch_clear_decimal_if_available();

    movement_location_t location = {0};
    filesystem_read_file("location.u32", (char *) &location.reg, sizeof(movement_location_t));
    if (location.reg == 0) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LOC", "L");
        watch_display_text(WATCH_POSITION_BOTTOM, "No LOC");
        return;
    }

    watch_date_time_t now = movement_get_local_date_time();
    uint32_t now_unix = watch_utility_date_time_to_unix_time(now, 0);

    if (!state->valid || now_unix >= state->span_end_unix) {
        _wadokei_compute_span(state, location, now_unix, now);
    }

    if (!state->valid) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "ERR", "E");
        watch_display_text(WATCH_POSITION_BOTTOM, "None  ");
        return;
    }

    // The current span (state->span_start_unix .. state->span_end_unix) is exactly one
    // full day-span (明六つ〜暮六つ) or one full night-span (暮六つ〜翌明六つ); divide it
    // into 24 equal "koku.csv pieces" and look up which branch/quarter that piece is.
    uint32_t span_len = state->span_end_unix - state->span_start_unix;
    uint32_t elapsed = now_unix - state->span_start_unix;
    uint32_t piece_len = span_len / 24;
    uint8_t piece_index = (piece_len > 0) ? (uint8_t)(elapsed / piece_len) : 0;
    if (piece_index > 23) piece_index = 23;

    uint8_t branch_index = state->is_daytime ? day_piece_branch[piece_index] : night_piece_branch[piece_index];
    uint8_t quarter_index = state->is_daytime ? day_piece_quarter[piece_index] : night_piece_quarter[piece_index];

    if (state->mode == 1) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *)branch_names[branch_index], (char *)branch_names[branch_index]);
        watch_display_text(WATCH_POSITION_BOTTOM, (char *)quarter_names[quarter_index]);
        return;
    }

    const char *prefix = koku_prefix[branch_index];
    uint8_t digit = koku_digit[branch_index];
    bool half = quarter_index >= 2;
    char buf[7];
    snprintf(buf, sizeof(buf), half ? "%dTUHAN" : "%dTU   ", digit);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, (char *)prefix, (char *)prefix);
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

void wadokei_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(wadokei_state_t));
        memset(*context_ptr, 0, sizeof(wadokei_state_t));
    }
}

void wadokei_face_activate(void *context) {
    if (watch_sleep_animation_is_running()) watch_stop_sleep_animation();
    wadokei_state_t *state = (wadokei_state_t *)context;

    movement_location_t movement_location = {0};
    filesystem_read_file("location.u32", (char *) &movement_location.reg, sizeof(movement_location_t));
    state->working_latitude = _wadokei_struct_from_latlon(movement_location.bit.latitude);
    state->working_longitude = _wadokei_struct_from_latlon(movement_location.bit.longitude);

    // Force a fresh recompute on every activation: the cached span may be stale if the
    // user changed the location or timezone while this face wasn't active.
    state->valid = false;
    _wadokei_face_update(state);
}

bool wadokei_face_loop(movement_event_t event, void *context) {
    wadokei_state_t *state = (wadokei_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _wadokei_face_update(state);
            break;
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            if (event.event_type == EVENT_LOW_ENERGY_UPDATE && state->page == 0 && !watch_sleep_animation_is_running()) {
                watch_start_sleep_animation(1000);
            }
            if (state->page == 0) _wadokei_face_update(state);
            else _wadokei_update_settings_display(event, state);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            if (state->page) {
                state->active_digit++;
                if (state->active_digit > 4) {
                    state->active_digit = 0;
                    state->page = (state->page + 1) % 3;
                    _wadokei_update_location_register(state);
                }
                if (state->page == 0) {
                    movement_request_tick_frequency(1);
                    _wadokei_face_update(state);
                } else {
                    _wadokei_update_settings_display(event, state);
                }
            } else {
                // Not in settings: fall back to the normal LIGHT-button behavior
                // (illuminate the LED) instead of silently swallowing the event.
                movement_illuminate_led();
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->page) {
                _wadokei_advance_digit(state);
                _wadokei_update_settings_display(event, state);
            } else {
                state->mode = !state->mode;
                _wadokei_face_update(state);
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->page == 0) {
                state->page = 1;
                state->active_digit = 0;
                watch_clear_display();
                movement_request_tick_frequency(4);
                _wadokei_update_settings_display(event, state);
            } else {
                state->page = 0;
                state->active_digit = 0;
                movement_request_tick_frequency(1);
                _wadokei_update_location_register(state);
                _wadokei_face_update(state);
            }
            break;
        case EVENT_TIMEOUT:
            if (state->page) {
                state->page = 0;
                state->active_digit = 0;
                movement_request_tick_frequency(1);
                _wadokei_update_location_register(state);
                _wadokei_face_update(state);
            }
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void wadokei_face_resign(void *context) {
    wadokei_state_t *state = (wadokei_state_t *)context;
    state->mode = 0;
    state->page = 0;
    state->active_digit = 0;
    _wadokei_update_location_register(state);
    watch_clear_decimal_if_available();
}
