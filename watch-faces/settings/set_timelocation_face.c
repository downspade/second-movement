/*
 * MIT License
 *
 * Copyright (c) 2026
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
#include "set_timelocation_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "filesystem.h"
#include "zones.h"

#define PAGE_YEAR     0
#define PAGE_MONTH    1
#define PAGE_DAY      2
#define PAGE_TIMEZONE 3
#define PAGE_CITY     4
#define PAGE_LATLON   5
#define PAGE_HOUR     6
#define PAGE_MINUTE   7
#define PAGE_SECOND   8
#define NUM_PAGES     9

static const char _stl_titles[NUM_PAGES][6] = {"Year ", "Month", "Day  ", "     ", "City ", "     ", "Hour ", "Minut", "Secnd"};
static const char _stl_fallback_titles[NUM_PAGES][3] = {"YR", "MO", "DA", "  ", "CT", "  ", "HR", "M1", "SE"};

static bool _quick_ticks_running;
static int32_t _current_offset; // cached UTC offset (seconds) for the Timezone page, like set_time_face's own

// 24 whole-hour UTC offsets (-11..+12), most with five well-known cities spread across
// different regions so the city-select step is actually useful, not three cities in one
// country -- except -11 (SST) and -2 (FNT), kept at three: worldwide, -11 has only American
// Samoa, Niue, and Midway Atoll, and -2 has only Fernando de Noronha, South Georgia, and
// Greenland (already this table's -2 pick via NUUK), so a fourth and fifth entry there would
// mean reaching for either tiny villages or a second city in a country already represented,
// rather than another notable, distinct place. Coordinates are hundredths of a degree, same
// encoding as movement_location_t -- ordinary well-known city locations, not precision-
// critical, so these are hand-entered from general knowledge rather than sourced/cross-
// checked; every entry's offset was, however, cross-checked against utz/zones.c's actual
// zone_defns base offsets (see _stl_zone_index_for_utz's comment) to confirm each city
// genuinely sits at the UTC offset its zone claims (standard time, ignoring DST, consistent
// with this table having no DST concept of its own). Some zone abbreviations are truncated to
// 3 characters from their usual 4-letter form (AKST->AKS, AZOT->AZO, AEST->AES, NZST->NZS);
// UTC+8's zone is labeled SGT rather than the also-common-but-ambiguous CST, since -6 (US
// Central) already uses that abbreviation here. (The abbreviations themselves aren't shown on
// screen by this face -- City's own header is what's displayed -- but they're kept for
// readability of this table and in case a future screen wants them.)
typedef struct {
    char name[7];
    int16_t latitude;
    int16_t longitude;
} location_city_t;

typedef struct {
    char abbr[4];
    int8_t utc_offset;
    const location_city_t *cities;
    uint8_t num_cities;
    uint8_t utz_index; // index into utz's zone_defns[], for movement_set_timezone_index()
} location_timezone_t;

static const location_city_t _cities_m11[] = { {"PAGOPA", -1428, -17070}, {"ALOFI", -1906, -16992}, {"MIDWAY", 2821, -17735} };
static const location_city_t _cities_m10[] = { {"HONOLU", 2131, -15786}, {"PAPEET", -1753, -14957}, {"ADAK", 5188, -17666}, {"RAROTO", -2124, -15978}, {"HILO", 1973, -15509} };
static const location_city_t _cities_m9[]  = { {"ANCHOR", 6122, -14990}, {"FAIRBK", 6484, -14772}, {"JUNEAU", 5830, -13442}, {"SITKA", 5705, -13533}, {"KODIAK", 5779, -15241} };
static const location_city_t _cities_m8[]  = { {"LOSANG", 3405, -11824}, {"VANCVR", 4928, -12312}, {"TIJUAN", 3251, -11702}, {"SANFRA", 3777, -12242}, {"SEATLE", 4761, -12233} };
static const location_city_t _cities_m7[]  = { {"DENVER", 3974, -10499}, {"PHOENX", 3345, -11207}, {"CALGAR", 5105, -11407}, {"ALBUQU", 3508, -10665}, {"SALTLK", 4076, -11189} };
static const location_city_t _cities_m6[]  = { {"CHICAG", 4188, -8763}, {"MEXICO", 1943, -9913}, {"WINNIP", 4990, -9714}, {"HOUSTN", 2976, -9537}, {"GUATEM", 1463, -9051} };
static const location_city_t _cities_m5[]  = { {"NEWYRK", 4071, -7401}, {"TORONT", 4365, -7938}, {"LIMA", -1205, -7704}, {"BOGOTA", 471, -7407}, {"MIAMI", 2576, -8019} };
static const location_city_t _cities_m4[]  = { {"HALIFX", 4465, -6357}, {"SANTIA", -3345, -7065}, {"CARACA", 1049, -6688}, {"LAPAZ", -1650, -6815}, {"SANJUA", 1847, -6611} };
static const location_city_t _cities_m3[]  = { {"SAOPAU", -2355, -4663}, {"BSAIRE", -3460, -5838}, {"MONTEV", -3490, -5616}, {"RIOJAN", -2291, -4317}, {"CAYENN", 492, -5231} };
static const location_city_t _cities_m2[]  = { {"NORONH", -385, -3242}, {"SGEORG", -5428, -3650}, {"NUUK", 6417, -5171} };
static const location_city_t _cities_m1[]  = { {"AZORES", 3774, -2567}, {"PRAIA", 1493, -2351}, {"SCORES", 7048, -2197}, {"MINDEL", 1689, -2498}, {"ANGRA", 3866, -2722} };
static const location_city_t _cities_0[]   = { {"LONDON", 5151, -13}, {"REYKJV", 6415, -2194}, {"ACCRA", 560, -19}, {"DAKAR", 1469, -1745}, {"BAMAKO", 1264, -800} };
static const location_city_t _cities_p1[]  = { {"PARIS", 4886, 235}, {"BERLIN", 5252, 1340}, {"LAGOS", 652, 338}, {"MADRID", 4042, -370}, {"ALGIER", 3675, 306} };
static const location_city_t _cities_p2[]  = { {"CAIRO", 3004, 3124}, {"ATHENS", 3798, 2373}, {"JOBURG", -2620, 2805}, {"HARARE", -1783, 3105}, {"HELSNK", 6017, 2494} };
static const location_city_t _cities_p3[]  = { {"MOSCOW", 5576, 3762}, {"NAIROB", -129, 3682}, {"RIYADH", 2471, 4668}, {"ADDIS", 903, 3875}, {"KUWAIT", 2938, 4798} };
static const location_city_t _cities_p4[]  = { {"DUBAI", 2520, 5527}, {"BAKU", 4041, 4987}, {"TBILIS", 4172, 4479}, {"MUSCAT", 2359, 5838}, {"YEREVN", 4018, 4450} };
static const location_city_t _cities_p5[]  = { {"KARACH", 2486, 6701}, {"TASHKN", 4130, 6924}, {"YEKATR", 5684, 6061}, {"DUSHAN", 3856, 6879}, {"ASHGAB", 3796, 5833} };
static const location_city_t _cities_p6[]  = { {"DHAKA", 2381, 9041}, {"ALMATY", 4324, 7689}, {"OMSK", 5499, 7337}, {"THIMPH", 2747, 8964}, {"BISHKK", 4287, 7457} };
static const location_city_t _cities_p7[]  = { {"BANGKK", 1376, 10050}, {"JAKART", -621, 10685}, {"HOCHIM", 1082, 10663}, {"HANOI", 2103, 10583}, {"PHNOMP", 1156, 10493} };
static const location_city_t _cities_p8[]  = { {"BEIJIN", 3990, 11641}, {"SINGAP", 135, 10382}, {"PERTH", -3195, 11586}, {"MANILA", 1460, 12098}, {"TAIPEI", 2503, 12157} };
static const location_city_t _cities_p9[]  = { {"TOKYO", 3568, 13965}, {"SEOUL", 3757, 12698}, {"KOROR", 734, 13448}, {"OSAKA", 3469, 13550}, {"PYONGY", 3904, 12576} };
static const location_city_t _cities_p10[] = { {"SYDNEY", -3387, 15121}, {"GUAM", 1347, 14475}, {"VLADIV", 4312, 13189}, {"BRISBN", -2747, 15303}, {"MORESB", -944, 14718} };
static const location_city_t _cities_p11[] = { {"HONIAR", -943, 15995}, {"NOUMEA", -2228, 16646}, {"MAGADA", 5956, 15080}, {"SAKHLN", 4696, 14274}, {"PORTVL", -1773, 16833} };
static const location_city_t _cities_p12[] = { {"AUCKLD", -3685, 17476}, {"SUVA", -1814, 17844}, {"PETROP", 5302, 15865}, {"WELLGN", -4129, 17478}, {"TARAWA", 134, 17297} };

#define ZONE(abbr_, offset_, cities_, utz_) { abbr_, offset_, cities_, sizeof(cities_) / sizeof(location_city_t), utz_ }

// Starts at London (GMT) and runs eastward (increasing UTC offset), wrapping from +12 around to
// -11 and back up to -1, rather than a plain ascending -11..+12 sort.
//
// utz_index picks, for each whole-hour offset here, a real named zone from utz/zones.h so that
// confirming a zone can also call movement_set_timezone_index() -- preferring a same-named/same-
// abbreviation zone where one exists (e.g. MSK->Moscow, JST->Tokyo), and among same-offset
// candidates preferring one that doesn't observe DST, since this face's own offset list has no
// DST concept. utz has no zone at exactly +11 or -1, so SBT and AZO fall back to the nearest
// available zone (Guam/+10 and London/0 respectively); picking either neighbor is equally
// "wrong" by an hour, so which way was arbitrary. AZO specifically falls back to London rather
// than UTC: every utz_index here must stay unique, since _stl_zone_index_for_utz() reverse-maps
// a utz_index back to a single row -- reusing UTC (already GMT's own utz_index) would make a
// confirmed AZO always redisplay as GMT.
static const location_timezone_t location_timezones[] = {
    ZONE("GMT", 0, _cities_0, UTZ_UTC),
    ZONE("CET", 1, _cities_p1, UTZ_LAGOS),
    ZONE("EET", 2, _cities_p2, UTZ_MAPUTO),
    ZONE("MSK", 3, _cities_p3, UTZ_MOSCOW),
    ZONE("GST", 4, _cities_p4, UTZ_DUBAI),
    ZONE("PKT", 5, _cities_p5, UTZ_KOLKATA),  // no exact +5 zone; Kolkata is +5:30
    ZONE("BDT", 6, _cities_p6, UTZ_KATHMANDU), // no exact +6 zone; Kathmandu is +5:45
    ZONE("ICT", 7, _cities_p7, UTZ_BANGKOK),
    ZONE("SGT", 8, _cities_p8, UTZ_SINGAPORE),
    ZONE("JST", 9, _cities_p9, UTZ_TOKYO),
    ZONE("AES", 10, _cities_p10, UTZ_BRISBANE),
    ZONE("SBT", 11, _cities_p11, UTZ_GUAM),   // no +11 zone in utz; nearest is Guam at +10
    ZONE("NZS", 12, _cities_p12, UTZ_AUCKLAND),
    ZONE("SST", -11, _cities_m11, UTZ_PAGO_PAGO),
    ZONE("HST", -10, _cities_m10, UTZ_HONOLULU),
    ZONE("AKS", -9, _cities_m9, UTZ_ANCHORAGE),
    ZONE("PST", -8, _cities_m8, UTZ_LOS_ANGELES),
    ZONE("MST", -7, _cities_m7, UTZ_PHOENIX),
    ZONE("CST", -6, _cities_m6, UTZ_REGINA),
    ZONE("EST", -5, _cities_m5, UTZ_NEW_YORK),
    ZONE("AST", -4, _cities_m4, UTZ_HALIFAX),
    ZONE("BRT", -3, _cities_m3, UTZ_SAO_PAULO),
    ZONE("FNT", -2, _cities_m2, UTZ_NUUK),
    ZONE("AZO", -1, _cities_m1, UTZ_LONDON),  // no -1 zone in utz; nearest is London at 0 (see above re: uniqueness)
};
#define NUM_TIMEZONES (sizeof(location_timezones) / sizeof(location_timezone_t))

#undef ZONE

// Reverse-maps a utz/zones.h index (from movement_get_timezone_index(), as just confirmed on
// the Timezone page) to its location_timezones[] entry, so City starts from whatever zone was
// actually just picked instead of always GMT. Falls back to 0 (GMT) if nothing here maps to it
// -- most fine-grained IANA zones won't have an exact match in this 24-entry table, since it
// only has one entry per whole-hour UTC offset.
static uint8_t _stl_zone_index_for_utz(uint8_t utz_index) {
    for (size_t i = 0; i < NUM_TIMEZONES; i++) {
        if (location_timezones[i].utz_index == utz_index) return (uint8_t) i;
    }
    return 0;
}

static void _stl_persist_location_to_filesystem(movement_location_t new_location) {
    movement_location_t maybe_location = {0};
    filesystem_read_file("location.u32", (char *) &maybe_location.reg, sizeof(movement_location_t));
    if (new_location.reg != maybe_location.reg) {
        filesystem_write_file("location.u32", (char *) &new_location.reg, sizeof(movement_location_t));
    }
}

static int16_t _stl_latlon_from_struct(set_timelocation_lat_lon_t val) {
    return (val.sign ? -1 : 1) *
           (val.hundreds * 10000 + val.tens * 1000 + val.ones * 100 + val.tenths * 10 + val.hundredths);
}

static set_timelocation_lat_lon_t _stl_struct_from_latlon(int16_t val) {
    set_timelocation_lat_lon_t retval = {0};
    retval.sign = val < 0;
    val = abs(val);
    retval.hundredths = val % 10; val /= 10;
    retval.tenths = val % 10; val /= 10;
    retval.ones = val % 10; val /= 10;
    retval.tens = val % 10; val /= 10;
    retval.hundreds = val % 10;
    return retval;
}

// Persists whatever location the current page implies: the working lat/lon digits at Lat/Lon,
// otherwise the highlighted city at City. Used both by Lat/Lon's own completion and by
// resigning mid-flow.
static void _stl_persist_current(set_timelocation_state_t *state) {
    movement_location_t new_location = {0};
    if (state->page == PAGE_LATLON) {
        new_location.bit.latitude = _stl_latlon_from_struct(state->working_latitude);
        new_location.bit.longitude = _stl_latlon_from_struct(state->working_longitude);
    } else {
        const location_city_t *city = &location_timezones[state->zone_index].cities[state->city_index];
        new_location.bit.latitude = city->latitude;
        new_location.bit.longitude = city->longitude;
    }
    _stl_persist_location_to_filesystem(new_location);
}

// Bumps the value of the currently active lat/lon digit (Alarm short press). Based on
// sunrise_sunset_face's _sunrise_sunset_face_advance_digit's custom-LCD branch, except
// latitude's tens digit wraps by subtracting exactly 90 past the limit instead of zeroing the
// sub-digits.
static void _stl_advance_digit_value(set_timelocation_state_t *state) {
    if (state->latlon_page == 0) { // latitude
        switch (state->active_digit) {
            case 0: {
                state->working_latitude.tens = (state->working_latitude.tens + 1) % 10;
                // Past 90 degrees, wrap the magnitude back by exactly 90 rather than
                // discarding the ones/tenths/hundredths digits already dialled in.
                int16_t magnitude = state->working_latitude.tens * 1000 + state->working_latitude.ones * 100 +
                                     state->working_latitude.tenths * 10 + state->working_latitude.hundredths;
                if (magnitude > 9000) {
                    magnitude -= 9000;
                    state->working_latitude.tens = (magnitude / 1000) % 10;
                    state->working_latitude.ones = (magnitude / 100) % 10;
                    state->working_latitude.tenths = (magnitude / 10) % 10;
                    state->working_latitude.hundredths = magnitude % 10;
                }
                break;
            }
            case 1:
                state->working_latitude.ones = (state->working_latitude.ones + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                break;
            case 2:
                state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                break;
            case 3:
                state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
                break;
            case 4:
                state->working_latitude.sign++;
                break;
        }
    } else { // longitude
        switch (state->active_digit) {
            case 0:
                state->working_longitude.tens++;
                if (state->working_longitude.tens >= 10) {
                    state->working_longitude.tens = 0;
                    state->working_longitude.hundreds++;
                }
                if (abs(_stl_latlon_from_struct(state->working_longitude)) > 18000) {
                    state->working_longitude.hundreds = 0;
                    state->working_longitude.tens = 0;
                    state->working_longitude.ones = 0;
                    state->working_longitude.tenths = 0;
                    state->working_longitude.hundredths = 0;
                }
                break;
            case 1:
                state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                break;
            case 2:
                state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                break;
            case 3:
                state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                if (abs(_stl_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                break;
            case 4:
                state->working_longitude.sign++;
                break;
        }
    }
}

// Zeroes out the currently active lat/lon digit (Alarm held). Longitude's digit 0 also clears
// the linked "hundreds" flag, mirroring how _stl_advance_digit_value sets it when tens rolls
// over past 9.
static void _stl_reset_digit_value(set_timelocation_state_t *state) {
    if (state->latlon_page == 0) { // latitude
        switch (state->active_digit) {
            case 0: state->working_latitude.tens = 0; break;
            case 1: state->working_latitude.ones = 0; break;
            case 2: state->working_latitude.tenths = 0; break;
            case 3: state->working_latitude.hundredths = 0; break;
            case 4: state->working_latitude.sign = 0; break;
        }
    } else { // longitude
        switch (state->active_digit) {
            case 0: state->working_longitude.tens = 0; state->working_longitude.hundreds = 0; break;
            case 1: state->working_longitude.ones = 0; break;
            case 2: state->working_longitude.tenths = 0; break;
            case 3: state->working_longitude.hundredths = 0; break;
            case 4: state->working_longitude.sign = 0; break;
        }
    }
}

// Handles a short Alarm press (or a quick-tick auto-repeat while held) on the date/timezone/
// time pages -- advances that page's value by one. Never called for City or Lat/Lon, which
// have their own Alarm handling (cycle-city / bump-digit) in the event loop below.
static void _stl_handle_datetime_alarm(watch_date_time_t date_time, set_timelocation_state_t *state) {
    switch (state->page) {
        case PAGE_TIMEZONE:
            movement_set_timezone_index(movement_get_timezone_index() + 1);
            if (movement_get_timezone_index() >= NUM_ZONE_NAMES) movement_set_timezone_index(0);
            _current_offset = movement_get_current_timezone_offset_for_zone(movement_get_timezone_index());
            return;
        case PAGE_YEAR:
            date_time.unit.year = (date_time.unit.year + 1) % 60;
            // e.g. Feb 29 in a leap year, advanced into a non-leap year: clamp the day down
            // to the new year's actual last day of that month, rather than let it silently
            // overflow into the next month.
            if (date_time.unit.day > watch_utility_days_in_month(date_time.unit.month, date_time.unit.year + WATCH_RTC_REFERENCE_YEAR)) {
                date_time.unit.day = watch_utility_days_in_month(date_time.unit.month, date_time.unit.year + WATCH_RTC_REFERENCE_YEAR);
            }
            break;
        case PAGE_MONTH:
            date_time.unit.month = (date_time.unit.month % 12) + 1;
            if (date_time.unit.day > watch_utility_days_in_month(date_time.unit.month, date_time.unit.year + WATCH_RTC_REFERENCE_YEAR)) {
                date_time.unit.day = watch_utility_days_in_month(date_time.unit.month, date_time.unit.year + WATCH_RTC_REFERENCE_YEAR);
            }
            break;
        case PAGE_DAY:
            date_time.unit.day = (date_time.unit.day % watch_utility_days_in_month(date_time.unit.month, date_time.unit.year + WATCH_RTC_REFERENCE_YEAR)) + 1;
            break;
        case PAGE_HOUR:
            date_time.unit.hour = (date_time.unit.hour + 1) % 24;
            break;
        case PAGE_MINUTE:
            date_time.unit.minute = (date_time.unit.minute + 1) % 60;
            break;
        case PAGE_SECOND:
            date_time.unit.second = 0;
            break;
    }
    movement_set_local_date_time(date_time);
}

static void _stl_abort_quick_ticks(void) {
    if (_quick_ticks_running) {
        _quick_ticks_running = false;
        movement_request_tick_frequency(4);
    }
}

static void _stl_update_display(movement_event_t event, set_timelocation_state_t *state, watch_date_time_t date_time) {
    char buf[11];
    uint8_t page = state->page;

    if (page == PAGE_CITY) {
        // City/Lat-Lon clear the whole display first (matching set_location_face's own
        // style) since their layout doesn't reuse every position the same way every tick;
        // the date/time/timezone branch below deliberately doesn't, so it never touches
        // indicators it has no opinion on (alarm bell, chime bell, low battery) -- matching
        // set_time_face's own style, which leaves those exactly as some other face left them.
        watch_clear_display();
        watch_display_text_with_fallback(WATCH_POSITION_TOP, "City ", "CT");
        const location_timezone_t *zone = &location_timezones[state->zone_index];
        sprintf(buf, "%-6s", zone->cities[state->city_index].name);
        watch_display_text(WATCH_POSITION_BOTTOM, buf);
        return;
    }

    if (page == PAGE_LATLON) {
        // same digit-by-digit editor as sunrise_sunset_face's/set_location_face's location setup
        watch_clear_display();
        if (state->latlon_page == 0) {
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "Latit", "LA");
            if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
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
            } else {
                // Same fallback shape as sunrise_sunset_face's classic branch: sign, a blank
                // (where custom's degree-sign/N-S pair would be), then the 4 digits as one
                // zero-padded number -- avoids the (0, 22) pixel below entirely, since it's
                // not a free pixel on classic (see set_location_face.c's fix for why).
                sprintf(buf, "%c %04d", state->working_latitude.sign ? '-' : '+', abs(_stl_latlon_from_struct(state->working_latitude)));
                if (event.subsecond % 2) buf[state->active_digit] = ' ';
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
        } else {
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "Longi", "LO");
            if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                watch_set_decimal_if_available();
                // Handle leading 1 for longitudes >99. (0, 22) is a free pixel on custom, but
                // NOT on classic -- it's classic's minutes-tens digit's A/D segment (see
                // Classic_LCD_Display_Mapping), so this whole branch is custom-only.
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
            } else {
                sprintf(buf, "%c%05d", state->working_longitude.sign ? '-' : '+', abs(_stl_latlon_from_struct(state->working_longitude)));
                if (event.subsecond % 2) buf[state->active_digit] = ' ';
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
        }
        return;
    }

    // Year / Month / Day / Timezone / Hour / Minute / Second -- same layout as set_time_face.
    // Lat/Lon's decimal point (a standalone pixel, not part of any digit glyph) is the one
    // thing that branch sets which nothing here would otherwise touch or overwrite -- clear it
    // explicitly so leaving Lat/Lon for Hour doesn't leave it lit on top of the new digits.
    watch_clear_decimal_if_available();
    watch_display_text(WATCH_POSITION_TOP_RIGHT, "  ");
    watch_display_text_with_fallback(WATCH_POSITION_TOP, (char *) _stl_titles[page], (char *) _stl_fallback_titles[page]);
    if (page == PAGE_TIMEZONE) {
        watch_display_text(WATCH_POSITION_TOP_RIGHT, " Z");
        if (_current_offset < 0) watch_display_text(WATCH_POSITION_TOP_LEFT, "- ");
        else watch_display_text(WATCH_POSITION_TOP_LEFT, "* ");
        if (event.subsecond % 2) {
            uint8_t hours = abs(_current_offset) / 3600;
            uint8_t minutes = (abs(_current_offset) % 3600) / 60;
            sprintf(buf, "%2d%02d  ", hours % 100, minutes % 100);
            watch_set_colon();
        } else {
            sprintf(buf, "%s", watch_utility_time_zone_name_at_index(movement_get_timezone_index()));
            watch_clear_colon();
        }
    } else if (page < PAGE_TIMEZONE) {
        watch_clear_colon();
        watch_clear_indicator(WATCH_INDICATOR_24H);
        watch_clear_indicator(WATCH_INDICATOR_PM);
        sprintf(buf, "%2d%02d%02d", date_time.unit.year + 20, date_time.unit.month, date_time.unit.day);
    } else {
        watch_set_colon();
        if (movement_clock_mode_24h()) {
            watch_set_indicator(WATCH_INDICATOR_24H);
            sprintf(buf, "%2d%02d%02d", date_time.unit.hour, date_time.unit.minute, date_time.unit.second);
        } else {
            sprintf(buf, "%2d%02d%02d", (date_time.unit.hour % 12) ? (date_time.unit.hour % 12) : 12, date_time.unit.minute, date_time.unit.second);
            if (date_time.unit.hour < 12) watch_clear_indicator(WATCH_INDICATOR_PM);
            else watch_set_indicator(WATCH_INDICATOR_PM);
        }
    }
    watch_display_text(WATCH_POSITION_BOTTOM, buf);

    // blink up the parameter we're setting
    if (event.subsecond % 2 && !_quick_ticks_running) {
        switch (page) {
            case PAGE_YEAR:
            case PAGE_HOUR:
                watch_display_text(WATCH_POSITION_HOURS, "  ");
                break;
            case PAGE_MONTH:
            case PAGE_MINUTE:
                watch_display_text(WATCH_POSITION_MINUTES, "  ");
                break;
            case PAGE_DAY:
            case PAGE_SECOND:
                watch_display_text(WATCH_POSITION_SECONDS, "  ");
                break;
        }
    }
}

void set_timelocation_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(set_timelocation_state_t));
        memset(*context_ptr, 0, sizeof(set_timelocation_state_t));
    }
}

void set_timelocation_face_activate(void *context) {
    set_timelocation_state_t *state = (set_timelocation_state_t *) context;
    memset(state, 0, sizeof(*state));
    movement_request_tick_frequency(4);
    _quick_ticks_running = false;
    _current_offset = movement_get_current_timezone_offset();
}

bool set_timelocation_face_loop(movement_event_t event, void *context) {
    set_timelocation_state_t *state = (set_timelocation_state_t *) context;
    watch_date_time_t date_time = movement_get_local_date_time();
    uint8_t page = state->page;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            break;
        case EVENT_TICK:
            if (_quick_ticks_running) {
                if (HAL_GPIO_BTN_ALARM_read()) _stl_handle_datetime_alarm(date_time, state);
                else _stl_abort_quick_ticks();
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (page == PAGE_CITY) {
                state->location_changed = true;
                state->city_index = 0;
            } else if (page == PAGE_LATLON) {
                state->location_changed = true;
                _stl_reset_digit_value(state);
            } else if (page != PAGE_SECOND) {
                // auto-repeat while held, same as set_time_face -- except Second, which (like
                // set_time_face's own page 6) only ever zeroes, so repeating it would do nothing.
                _quick_ticks_running = true;
                movement_request_tick_frequency(8);
            }
            break;
        case EVENT_ALARM_LONG_UP:
            _stl_abort_quick_ticks();
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            switch (page) {
                case PAGE_TIMEZONE:
                    // confirm the timezone -> jump into city select, seeded from the zone just picked
                    state->zone_index = _stl_zone_index_for_utz((uint8_t) movement_get_timezone_index());
                    state->city_index = 0;
                    state->page = PAGE_CITY;
                    break;
                case PAGE_CITY: {
                    // confirm the city -> seed the lat/lon editor with its coordinates
                    const location_city_t *city = &location_timezones[state->zone_index].cities[state->city_index];
                    state->working_latitude = _stl_struct_from_latlon(city->latitude);
                    state->working_longitude = _stl_struct_from_latlon(city->longitude);
                    state->latlon_page = 0;
                    state->active_digit = 0;
                    state->page = PAGE_LATLON;
                    break;
                }
                case PAGE_LATLON:
                    state->active_digit++;
                    if (state->active_digit > 4) {
                        state->active_digit = 0;
                        if (state->latlon_page == 0) {
                            state->latlon_page = 1; // move from latitude to longitude
                        } else {
                            // finished longitude: write it out and resume at Hour, right where
                            // the Timezone step left off.
                            _stl_persist_current(state);
                            state->location_changed = false;
                            state->latlon_page = 0;
                            state->page = PAGE_HOUR;
                        }
                    }
                    break;
                default:
                    state->page = (page + 1) % NUM_PAGES;
                    break;
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            _stl_abort_quick_ticks();
            if (page == PAGE_CITY) {
                state->location_changed = true;
                state->city_index = (state->city_index + 1) % location_timezones[state->zone_index].num_cities;
            } else if (page == PAGE_LATLON) {
                state->location_changed = true;
                _stl_advance_digit_value(state);
            } else {
                _stl_handle_datetime_alarm(date_time, state);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        case EVENT_TIMEOUT:
            _stl_abort_quick_ticks();
            // Leaving mid-flow shouldn't discard an in-progress city/lat-lon edit -- but only
            // commit if Alarm actually changed something this visit (same guard set_location_face
            // itself uses), so simply passing through this page without touching Alarm can't
            // clobber an already-set location with e.g. the zone's default first city.
            if ((page == PAGE_CITY || page == PAGE_LATLON) && state->location_changed) {
                _stl_persist_current(state);
                state->location_changed = false;
            }
            movement_move_to_face(0);
            break;
        default:
            return movement_default_loop_handler(event);
    }

    _stl_update_display(event, state, movement_get_local_date_time());
    return true;
}

void set_timelocation_face_resign(void *context) {
    set_timelocation_state_t *state = (set_timelocation_state_t *) context;
    movement_request_tick_frequency(1);
    _stl_abort_quick_ticks();

    if ((state->page == PAGE_CITY || state->page == PAGE_LATLON) && state->location_changed) {
        _stl_persist_current(state);
        state->location_changed = false;
    }

    // movement_set_timezone_index() only updates the in-RAM setting; without this it's never
    // written to settings.u32, so it would silently revert to whatever was last stored there on
    // the next boot. set_time_face_resign() and set_location_face_resign() both do this too on
    // every exit -- it's a no-op write if nothing actually changed.
    movement_store_settings();
}
