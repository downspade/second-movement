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
#include "set_location_face.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "filesystem.h"
#include "zones.h"

// Lights raw position 10 (the 3rd TOP_LEFT character) as a plus sign, for the "GT+" label.
static void _display_plus_at_position_10(void) {
    if (watch_get_lcd_type() != WATCH_LCD_TYPE_CUSTOM) return;
    digit_mapping_t segmap = Custom_LCD_Display_Mapping[10];
    for (int i = 0; i < 8; i++) {
        if (segmap.segment[i].value == segment_does_not_exist) continue;
        bool on = (i == 6 || i == 7); // G and H
        if (on) watch_set_pixel(segmap.segment[i].address.com, segmap.segment[i].address.seg);
        else watch_clear_pixel(segmap.segment[i].address.com, segmap.segment[i].address.seg);
    }
}

// 24 whole-hour UTC offsets (-11..+12), each with a handful of well-known cities spread across
// different regions so the city-select step is actually useful. Coordinates are hundredths of a
// degree (movement_location_t's encoding), hand-entered from general knowledge -- not
// precision-critical the way moon_phase_ascii_face's eclipse table is. Some zone abbreviations
// are truncated to 3 characters (AKST->AKS, AZOT->AZO, AEST->AES, NZST->NZS); UTC+8 is labeled
// SGT rather than the ambiguous CST, since -6 (US Central) already uses that abbreviation.
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
static const location_city_t _cities_m10[] = { {"HONOLU", 2131, -15786}, {"PAPEET", -1753, -14957}, {"ADAK", 5188, -17666} };
static const location_city_t _cities_m9[]  = { {"ANCHOR", 6122, -14990}, {"FAIRBK", 6484, -14772}, {"JUNEAU", 5830, -13442} };
static const location_city_t _cities_m8[]  = { {"LOSANG", 3405, -11824}, {"VANCVR", 4928, -12312}, {"TIJUAN", 3251, -11702} };
static const location_city_t _cities_m7[]  = { {"DENVER", 3974, -10499}, {"PHOENX", 3345, -11207}, {"CALGAR", 5105, -11407} };
static const location_city_t _cities_m6[]  = { {"CHICAG", 4188, -8763}, {"MEXICO", 1943, -9913}, {"WINNIP", 4990, -9714} };
static const location_city_t _cities_m5[]  = { {"NEWYRK", 4071, -7401}, {"TORONT", 4365, -7938}, {"LIMA", -1205, -7704} };
static const location_city_t _cities_m4[]  = { {"HALIFX", 4465, -6357}, {"SANTIA", -3345, -7065}, {"CARACA", 1049, -6688} };
static const location_city_t _cities_m3[]  = { {"SAOPAU", -2355, -4663}, {"BSAIRE", -3460, -5838}, {"MONTEV", -3490, -5616} };
static const location_city_t _cities_m2[]  = { {"NORONH", -385, -3242}, {"SGEORG", -5428, -3650}, {"NUUK", 6417, -5171} };
static const location_city_t _cities_m1[]  = { {"AZORES", 3774, -2567}, {"PRAIA", 1493, -2351}, {"SCORES", 7048, -2197} };
static const location_city_t _cities_0[]   = { {"LONDON", 5151, -13}, {"REYKJV", 6415, -2194}, {"ACCRA", 560, -19} };
static const location_city_t _cities_p1[]  = { {"PARIS", 4886, 235}, {"BERLIN", 5252, 1340}, {"LAGOS", 652, 338} };
static const location_city_t _cities_p2[]  = { {"CAIRO", 3004, 3124}, {"ATHENS", 3798, 2373}, {"JOBURG", -2620, 2805} };
static const location_city_t _cities_p3[]  = { {"MOSCOW", 5576, 3762}, {"NAIROB", -129, 3682}, {"RIYADH", 2471, 4668} };
static const location_city_t _cities_p4[]  = { {"DUBAI", 2520, 5527}, {"BAKU", 4041, 4987}, {"TBILIS", 4172, 4479} };
static const location_city_t _cities_p5[]  = { {"KARACH", 2486, 6701}, {"TASHKN", 4130, 6924}, {"YEKATR", 5684, 6061} };
static const location_city_t _cities_p6[]  = { {"DHAKA", 2381, 9041}, {"ALMATY", 4324, 7689}, {"OMSK", 5499, 7337} };
static const location_city_t _cities_p7[]  = { {"BANGKK", 1376, 10050}, {"JAKART", -621, 10685}, {"HOCHIM", 1082, 10663} };
static const location_city_t _cities_p8[]  = { {"BEIJIN", 3990, 11641}, {"SINGAP", 135, 10382}, {"PERTH", -3195, 11586} };
static const location_city_t _cities_p9[]  = { {"TOKYO", 3568, 13965}, {"SEOUL", 3757, 12698}, {"KOROR", 734, 13448} };
static const location_city_t _cities_p10[] = { {"SYDNEY", -3387, 15121}, {"GUAM", 1347, 14475}, {"VLADIV", 4312, 13189} };
static const location_city_t _cities_p11[] = { {"HONIAR", -943, 15995}, {"NOUMEA", -2228, 16646}, {"MAGADA", 5956, 15080} };
static const location_city_t _cities_p12[] = { {"AUCKLD", -3685, 17476}, {"SUVA", -1814, 17844}, {"PETROP", 5302, 15865} };

#define ZONE(abbr_, offset_, cities_, utz_) { abbr_, offset_, cities_, sizeof(cities_) / sizeof(location_city_t), utz_ }

// Starts at London (GMT) and runs eastward, wrapping from +12 to -11 and back up to -1, rather
// than a plain ascending sort.
//
// utz_index maps each offset to a real named zone from utz/zones.h so confirming a zone can
// call movement_set_timezone_index() -- preferring a same-name/abbreviation zone where one
// exists, and among same-offset candidates one without DST, since this face's offset list has
// no DST concept. utz has no zone at exactly +11 or -1, so SBT and AZO fall back to the nearest
// available zone (Guam/+10, London/0). AZO falls back to London rather than UTC because every
// utz_index here must stay unique -- _set_location_zone_index_for_utz() reverse-maps a
// utz_index to a single row, and reusing GMT's UTC would make a confirmed AZO redisplay as GMT.
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

// Reverse-maps a utz/zones.h index (from movement_get_timezone_index()) back to its
// location_timezones[] entry, so set_location_face_activate() can start the UI from whatever
// zone is actually currently set instead of always resetting to GMT. Falls back to 0 (GMT) if
// nothing here maps to it (e.g. a timezone set some other way, like set_time_face's own zone
// cycling) -- an imperfect but harmless default, same as before this reverse mapping existed.
static uint8_t _set_location_zone_index_for_utz(uint8_t utz_index) {
    for (size_t i = 0; i < NUM_TIMEZONES; i++) {
        if (location_timezones[i].utz_index == utz_index) return (uint8_t) i;
    }
    return 0;
}

static void persist_location_to_filesystem(movement_location_t new_location) {
    movement_location_t maybe_location = {0};
    filesystem_read_file("location.u32", (char *) &maybe_location.reg, sizeof(movement_location_t));
    if (new_location.reg != maybe_location.reg) {
        filesystem_write_file("location.u32", (char *) &new_location.reg, sizeof(movement_location_t));
    }
}

static int16_t _set_location_latlon_from_struct(set_location_lat_lon_settings_t val) {
    return (val.sign ? -1 : 1) *
           (val.hundreds * 10000 + val.tens * 1000 + val.ones * 100 + val.tenths * 10 + val.hundredths);
}

static set_location_lat_lon_settings_t _set_location_struct_from_latlon(int16_t val) {
    set_location_lat_lon_settings_t retval = {0};
    retval.sign = val < 0;
    val = abs(val);
    retval.hundredths = val % 10; val /= 10;
    retval.tenths = val % 10; val /= 10;
    retval.ones = val % 10; val /= 10;
    retval.tens = val % 10; val /= 10;
    retval.hundreds = val % 10;
    return retval;
}

// Persists whatever location the current stage implies: the working lat/lon digits at
// stage 2, otherwise the zone's highlighted city (stage 1) or its representative first
// city (stage 0). Used both by stage 2's own completion and by resigning mid-flow.
static void _set_location_persist_current(set_location_state_t *state) {
    const location_timezone_t *zone = &location_timezones[state->zone_index];
    movement_location_t new_location = {0};
    if (state->stage == 2) {
        new_location.bit.latitude = _set_location_latlon_from_struct(state->working_latitude);
        new_location.bit.longitude = _set_location_latlon_from_struct(state->working_longitude);
    } else {
        const location_city_t *city = &zone->cities[(state->stage == 1) ? state->city_index : 0];
        new_location.bit.latitude = city->latitude;
        new_location.bit.longitude = city->longitude;
    }
    persist_location_to_filesystem(new_location);
}

// Bumps the value of the currently active digit (stage 2, Alarm short press). Based on
// sunrise_sunset_face's _sunrise_sunset_face_advance_digit's custom-LCD branch (this face is
// custom-LCD-only, so there's no classic-LCD branch to carry over), except latitude's tens
// digit wraps by subtracting exactly 90 past the limit instead of zeroing the sub-digits.
static void _set_location_advance_digit_value(set_location_state_t *state) {
    if (state->page == 0) { // latitude
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
                if (abs(_set_location_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                break;
            case 2:
                state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                if (abs(_set_location_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                break;
            case 3:
                state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                if (abs(_set_location_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
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
                if (abs(_set_location_latlon_from_struct(state->working_longitude)) > 18000) {
                    state->working_longitude.hundreds = 0;
                    state->working_longitude.tens = 0;
                    state->working_longitude.ones = 0;
                    state->working_longitude.tenths = 0;
                    state->working_longitude.hundredths = 0;
                }
                break;
            case 1:
                state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                if (abs(_set_location_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                break;
            case 2:
                state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                if (abs(_set_location_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                break;
            case 3:
                state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                if (abs(_set_location_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                break;
            case 4:
                state->working_longitude.sign++;
                break;
        }
    }
}

// Zeroes out the currently active digit (stage 2, Alarm held). Longitude's digit 0 also
// clears the linked "hundreds" flag, mirroring how _set_location_advance_digit_value sets
// it when tens rolls over past 9.
static void _set_location_reset_digit_value(set_location_state_t *state) {
    if (state->page == 0) { // latitude
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

static void _set_location_update_display(movement_event_t event, set_location_state_t *state) {
    char buf[8];
    const location_timezone_t *zone = &location_timezones[state->zone_index];

    watch_clear_display();

    switch (state->stage) {
        case 0: { // timezone select
            // _with_fallback is needed to reach TOP_LEFT's 3rd character; same reason "LAT"/"LON" below need it.
            if (zone->utc_offset > 0) {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "GT ", "G+");
                _display_plus_at_position_10();
            } else if (zone->utc_offset < 0) {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "GT-", "G-");
            } else {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "GMT", "GM");
            }
            sprintf(buf, "%-2d", abs(zone->utc_offset));
            watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
            // preview the zone's first candidate city, using the full 6-character width
            sprintf(buf, "%-6s", zone->cities[0].name);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            break;
        }
        case 1: { // city select -- same top, city name on the bottom
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, zone->abbr, zone->abbr);
            sprintf(buf, "%-6s", zone->cities[state->city_index].name);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            break;
        }
        case 2: { // lat/lon fine-tune -- same layout as sunrise_sunset_face's location setup
            if (state->page == 0) {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LAT", "LA");
                if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                    watch_set_decimal_if_available();
                    watch_display_character('0' + state->working_latitude.tens, 4);
                    watch_display_character('0' + state->working_latitude.ones, 5);
                    watch_display_character('0' + state->working_latitude.tenths, 6);
                    watch_display_character('0' + state->working_latitude.hundredths, 7);
                    watch_display_character('#', 8);
                    watch_display_character(state->working_latitude.sign ? 'S' : 'N', 9);
                    if (event.subsecond % 2) {
                        // active_digit==4 is the N/S sign -- blink only position 9, not the '#' at 8.
                        if (state->active_digit == 4) watch_display_character(' ', 9);
                        else watch_display_character(' ', 4 + state->active_digit);
                    }
                } else {
                    // Classic: sign, a blank, then the 4 digits as one zero-padded number.
                    sprintf(buf, "%c %04d", state->working_latitude.sign ? '-' : '+', abs(_set_location_latlon_from_struct(state->working_latitude)));
                    // buf[0] is the sign, buf[2..5] the 4 digits.
                    if (event.subsecond % 2) buf[state->active_digit == 4 ? 0 : state->active_digit + 2] = ' ';
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                }
            } else {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LON", "LO");
                if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                    watch_set_decimal_if_available();
                    // Handle leading 1 for longitudes >99 (custom only).
                    if (state->working_longitude.hundreds == 1) watch_set_pixel(0, 22);
                    watch_display_character('0' + state->working_longitude.tens, 4);
                    watch_display_character('0' + state->working_longitude.ones, 5);
                    watch_display_character('0' + state->working_longitude.tenths, 6);
                    watch_display_character('0' + state->working_longitude.hundredths, 7);
                    watch_display_character('#', 8);
                    watch_display_character(state->working_longitude.sign ? 'W' : 'E', 9);
                    if (event.subsecond % 2) {
                        // active_digit==4 is the E/W sign -- blink only position 9, not the '#' at 8.
                        if (state->active_digit == 4) {
                            watch_display_character(' ', 9);
                        } else {
                            watch_display_character(' ', 4 + state->active_digit);
                            if (state->active_digit == 0) watch_clear_pixel(0, 22);
                        }
                    }
                } else {
                    sprintf(buf, "%c%05d", state->working_longitude.sign ? '-' : '+', abs(_set_location_latlon_from_struct(state->working_longitude)));
                    if (event.subsecond % 2) buf[state->active_digit == 4 ? 0 : state->active_digit + 2] = ' ';
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                }
            }
            break;
        }
    }
}

void set_location_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(set_location_state_t));
        memset(*context_ptr, 0, sizeof(set_location_state_t));
    }
}

void set_location_face_activate(void *context) {
    set_location_state_t *state = (set_location_state_t *) context;
    if (watch_sleep_animation_is_running()) watch_stop_sleep_animation();
    // Always start over at timezone select -- but from whichever zone is actually currently
    // set (reverse-mapped from movement's timezone index), not always GMT. Without this,
    // leaving the face after confirming e.g. Tokyo and coming straight back would show GMT
    // again, looking like the change hadn't taken -- even though it had.
    state->stage = 0;
    state->zone_index = _set_location_zone_index_for_utz((uint8_t) movement_get_timezone_index());
    state->city_index = 0;
    state->page = 0;
    state->active_digit = 0;
    movement_request_tick_frequency(1);
}

bool set_location_face_loop(movement_event_t event, void *context) {
    set_location_state_t *state = (set_location_state_t *) context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _set_location_update_display(event, state);
            break;
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            // Only stage 2 animates (the blinking active digit); stages 0/1 are static.
            if (state->stage == 2) _set_location_update_display(event, state);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            // Without this, the event falls through to movement_default_loop_handler, which
            // illuminates the LED immediately on every Light press -- short or long. Swallowing
            // it here keeps illumination to EVENT_LIGHT_LONG_PRESS only, below.
            break;
        case EVENT_ALARM_BUTTON_UP: // advance: next zone / next city / bump digit value
            state->changed = true;
            switch (state->stage) {
                case 0:
                    state->zone_index = (state->zone_index + 1) % NUM_TIMEZONES;
                    break;
                case 1:
                    state->city_index = (state->city_index + 1) % location_timezones[state->zone_index].num_cities;
                    break;
                case 2:
                    _set_location_advance_digit_value(state);
                    break;
            }
            _set_location_update_display(event, state);
            break;
        case EVENT_LIGHT_BUTTON_UP: // next item: next stage, or (stage 2) next digit/page
            switch (state->stage) {
                case 0:
                    movement_set_timezone_index(location_timezones[state->zone_index].utz_index);
                    // Only overwrite location.u32 with the zone's first city if Alarm actually
                    // picked a zone this visit. Since activate() now starts zone_index at the
                    // wearer's actual current zone (not always GMT), simply opening this face
                    // to check the current zone and pressing Light out of habit must not
                    // clobber an already fine-tuned location with the zone's generic city.
                    if (state->changed) {
                        _set_location_persist_current(state);
                        state->changed = false;
                    }
                    state->stage = 1;
                    state->city_index = 0;
                    break;
                case 1: {
                    // Linked: seed the lat/lon editor from the chosen city's coordinates.
                    const location_city_t *city = &location_timezones[state->zone_index].cities[state->city_index];
                    state->working_latitude = _set_location_struct_from_latlon(city->latitude);
                    state->working_longitude = _set_location_struct_from_latlon(city->longitude);
                    state->page = 0;
                    state->active_digit = 0;
                    state->stage = 2;
                    movement_request_tick_frequency(4);
                    break;
                }
                case 2:
                    state->active_digit++;
                    if (state->active_digit > 4) {
                        state->active_digit = 0;
                        if (state->page == 0) {
                            state->page = 1; // move from latitude to longitude
                        } else {
                            // finished longitude: write it out and go back to timezone select
                            _set_location_persist_current(state);
                            state->changed = false;
                            movement_request_tick_frequency(1);
                            state->stage = 0;
                            state->page = 0;
                        }
                    }
                    break;
            }
            _set_location_update_display(event, state);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        case EVENT_ALARM_LONG_PRESS: // reset the current value to 0: zone, city, or active digit
            state->changed = true;
            switch (state->stage) {
                case 0:
                    state->zone_index = 0;
                    break;
                case 1:
                    state->city_index = 0;
                    break;
                case 2:
                    _set_location_reset_digit_value(state);
                    break;
            }
            _set_location_update_display(event, state);
            break;
        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void set_location_face_resign(void *context) {
    set_location_state_t *state = (set_location_state_t *) context;
    movement_request_tick_frequency(1);

    // Leaving mid-flow (Mode, short or long) shouldn't discard an in-progress edit -- but
    // only commit if Alarm actually changed something this visit. Without that guard, just
    // passing through this face via Mode (without touching Alarm) would overwrite an
    // already-set location with e.g. the default zone's representative city every time.
    if (state->changed) {
        movement_set_timezone_index(location_timezones[state->zone_index].utz_index);
        _set_location_persist_current(state);
        state->changed = false;
    }

    // movement_set_timezone_index() (here and at the stage-0 confirm in the loop handler
    // above) only updates the in-RAM setting; without this it's never written to
    // settings.u32, so it would silently revert to whatever was last stored there on the
    // next boot. set_time_face_resign() and settings_face_resign() both do this too on
    // every exit -- it's a no-op write if nothing actually changed.
    movement_store_settings();

    state->stage = 0;
    state->page = 0;
    state->active_digit = 0;
}
