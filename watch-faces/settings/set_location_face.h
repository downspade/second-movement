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

#ifndef SET_LOCATION_FACE_H_
#define SET_LOCATION_FACE_H_

/*
 * SET LOCATION face (custom LCD only)
 *
 * Sets the wearer's location (the same location.u32 that sunrise_sunset_face,
 * wadokei_face, and moon_phase_ascii_face all read) through three stages,
 * rather than dialing in raw lat/lon from scratch: pick a timezone, pick a
 * nearby city in it (which seeds the lat/lon editor with that city's
 * coordinates), then fine-tune from there. The city step and the lat/lon
 * step are linked -- picking a city is just a fast way to get the digit
 * editor close to the right answer before nudging it exact.
 *
 * Button mapping (custom LCD's top area only ever shows a plain 3-letter
 * label -- no numbers, no borrowed indicators -- so the current zone
 * abbreviation or LAT/LON is all that's up there):
 *  - Alarm (short): advance -- next zone, next city, or bump the active
 *    digit's value, depending on stage.
 *  - Light (short): move to the next item -- next stage (0->1->2), or
 *    within stage 2, the next digit (rolling latitude into longitude after
 *    its last digit, then writing location.u32 and returning to stage 1
 *    after longitude's last digit).
 *  - Alarm (held): reset the current value to 0 -- zone, city, or the
 *    active digit, depending on stage.
 *  - Light (held): illuminate the display, same as everywhere else.
 * There's no "go back" (previous zone/city, or a lower digit value) --
 * Alarm (short) wraps around, so cycling forward always reaches any value.
 * There's also no "go back a stage" yet -- Mode (which just moves to the
 * next face, as usual) is the only way out mid-flow for now.
 *
 * Stage 0 -- timezone: top-left shows "GT+"/"GT-"/"GMT" for the zone's sign
 * (positive/negative/zero UTC offset), top-right its magnitude in hours;
 * bottom row previews the zone's first candidate city. Light confirms the
 * zone -- also calling movement_set_timezone_index() with the utz/zones.h
 * entry this zone maps to (see location_timezones[]'s utz_index), so the
 * system's actual timezone (used for local-time display and DST) tracks
 * what was picked here, not just location.u32's lat/lon -- and moves to
 * stage 1.
 *
 * Stage 1 -- city: top-left keeps the zone's 3-letter abbreviation (e.g.
 * JST, PST, GMT) for context; bottom row shows one of a few candidate
 * cities in that zone. Light
 * confirms -- seeding the lat/lon editor with that city's coordinates
 * (the "linked" part: picking a city is just a fast way to get the digit
 * editor close to the right answer before nudging it exact) -- and moves
 * to stage 2.
 *
 * Stage 2 -- lat/lon fine-tune: same digit-by-digit editor as
 * sunrise_sunset_face's location setup (latitude, then longitude).
 *
 * Leaving mid-flow (Mode, short or long) doesn't discard the in-progress edit: as long as
 * Alarm has actually changed something this visit (state->changed), resign writes out
 * whatever's currently selected -- the zone's representative (first) city at stage 0, the
 * highlighted city at stage 1, or the working lat/lon as edited so far at stage 2 -- the
 * same way stage 2's own completion does. The state->changed guard (rather than always
 * writing on resign) exists so that simply passing through this face via Mode, without
 * touching Alarm, can't clobber an already-set location with e.g. the default zone's city.
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
