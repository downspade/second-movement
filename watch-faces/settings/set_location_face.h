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

#ifndef SET_LOCATION_FACE_H_
#define SET_LOCATION_FACE_H_

/*
 * SET LOCATION face
 *
 * Sets the wearer's location (the same location.u32 that sunrise_sunset_face, wadokei_face,
 * and moon_phase_ascii_face all read) through three linked stages, rather than dialing in raw
 * lat/lon from scratch: pick a timezone, pick a nearby city in it (seeding the lat/lon editor
 * with that city's coordinates), then fine-tune from there.
 *
 * Both display types are supported: top-row labels have two-character fallbacks on classic,
 * and the lat/lon editor there uses the same shape as sunrise_sunset_face's classic branch.
 *
 * Button mapping:
 *  - Alarm (short): advance -- next zone, next city, or bump the active digit's value.
 *  - Light (short): next stage (0->1->2), or within stage 2, the next digit (latitude rolls
 *    into longitude, then longitude's last digit writes location.u32 and returns to stage 0).
 *  - Alarm (held): reset the current value to 0 -- zone, city, or the active digit.
 *  - Light (held): illuminate the display, same as everywhere else.
 * No "go back" -- Alarm (short) wraps around, reaching any value by cycling forward. No
 * "go back a stage" either -- Mode is the only way out mid-flow for now.
 *
 * Stage 0 -- timezone: top-left shows "GT+"/"GT-"/"GMT" for the zone's UTC-offset sign,
 * top-right its magnitude in hours, bottom row previews the zone's first candidate city.
 * Light confirms, also calling movement_set_timezone_index() (see location_timezones[]'s
 * utz_index) so the system's actual timezone tracks what was picked here, not just
 * location.u32's lat/lon.
 *
 * Stage 1 -- city: top-left keeps the zone's abbreviation for context; bottom row shows a
 * candidate city. Light confirms, seeding the lat/lon editor with its coordinates.
 *
 * Stage 2 -- lat/lon fine-tune: same digit-by-digit editor as sunrise_sunset_face's location
 * setup (latitude, then longitude).
 *
 * Leaving mid-flow (Mode) doesn't discard the in-progress edit: as long as Alarm changed
 * something this visit (state->changed), resign writes out whatever's currently selected, the
 * same way stage 2's own completion does. The guard exists so simply passing through this face
 * via Mode, without touching Alarm, can't clobber an already-set location.
 */

#include "movement.h"

typedef struct {
    uint8_t sign : 1;
    uint8_t hundreds : 5;
    uint8_t tens : 5;
    uint8_t ones : 4;
    uint8_t tenths : 4;
    uint8_t hundredths : 4;
} set_location_lat_lon_settings_t;

typedef struct {
    uint8_t stage;       // 0 = timezone, 1 = city, 2 = lat/lon
    uint8_t zone_index;  // 0-23, into location_timezones[]
    uint8_t city_index;  // 0-based, into the chosen zone's cities[]
    uint8_t page;        // within stage 2: 0 = latitude, 1 = longitude
    uint8_t active_digit;
    bool changed;        // true once Alarm has actually advanced/reset something this visit
    set_location_lat_lon_settings_t working_latitude;
    set_location_lat_lon_settings_t working_longitude;
} set_location_state_t;

void set_location_face_setup(uint8_t watch_face_index, void ** context_ptr);
void set_location_face_activate(void *context);
bool set_location_face_loop(movement_event_t event, void *context);
void set_location_face_resign(void *context);

#define set_location_face ((const watch_face_t){ \
    set_location_face_setup, \
    set_location_face_activate, \
    set_location_face_loop, \
    set_location_face_resign, \
    NULL, \
})

#endif // SET_LOCATION_FACE_H_
