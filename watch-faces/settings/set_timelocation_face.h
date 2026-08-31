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

#ifndef SET_TIMELOCATION_FACE_H_
#define SET_TIMELOCATION_FACE_H_

/*
 * SET TIME+LOCATION face (custom LCD only)
 *
 * A single guided setup flow covering everything set_time_face and set_location_face cover
 * between them -- date, timezone, location, and time of day -- in one pass. This is a
 * self-contained face: it doesn't call into set_time_face.c or set_location_face.c, it has
 * its own copy of the date/time and city/lat-lon logic those faces use, so this face can be
 * dropped into another project as this .c/.h pair alone, with no dependency on either of them
 * being present.
 *
 * Nine pages, advanced with Light (short press):
 *
 *   0 Year   1 Month   2 Day   3 Timezone   4 City   5 Lat/Lon   6 Hour   7 Minute   8 Second
 *
 * Pages 0-3 and 6-8 work exactly like set_time_face's own fields: Alarm (short) bumps the
 * value; Alarm (held) auto-repeats that bump for as long as it's held (except on Second,
 * which only zeroes -- same exception set_time_face itself makes). Confirming Timezone (page
 * 3) reverse-maps whatever zone was just picked into one of this face's 24 whole-hour
 * zone/city entries (falling back to GMT if no exact match exists -- most fine-grained IANA
 * zones won't have one, since this table only has one entry per whole-hour UTC offset) and
 * moves to City instead of Hour.
 *
 * Page 4 (City) and page 5 (Lat/Lon) work like set_location_face's own stages 1 and 2: Alarm
 * (short) cycles to the next candidate city, or bumps the active lat/lon digit; Alarm (held)
 * resets the current one (city or digit) instead of auto-repeating. Light on City seeds the
 * lat/lon editor with the chosen city's coordinates and moves to Lat/Lon; Light on Lat/Lon
 * advances one digit at a time (latitude's 5 digits, then longitude's 5), writing location.u32
 * once longitude's last digit is confirmed and moving on to Hour (page 6) -- resuming exactly
 * where the Timezone step left off.
 *
 * Leaving mid-flow (Mode, or idle timeout) always saves the date/time and timezone settings
 * (matching set_time_face's own every-exit save), and additionally writes location.u32 if
 * Alarm actually changed the city or a lat/lon digit on the current page this visit --
 * matching set_location_face's own guard against clobbering an already-set location just by
 * passing through without touching Alarm.
 */

#include "movement.h"

typedef struct {
    uint8_t sign : 1;
    uint8_t hundreds : 5;
    uint8_t tens : 5;
    uint8_t ones : 4;
    uint8_t tenths : 4;
    uint8_t hundredths : 4;
} set_timelocation_lat_lon_t;

typedef struct {
    uint8_t page;         // 0-8, see the page list above
    uint8_t zone_index;   // into this face's own 24-entry zone/city table, valid from City onward
    uint8_t city_index;   // 0-based, into the chosen zone's candidate cities
    uint8_t latlon_page;  // within Lat/Lon: 0 = latitude, 1 = longitude
    uint8_t active_digit; // within Lat/Lon: 0-4
    bool location_changed; // true once Alarm has actually changed the city or a digit this visit
    set_timelocation_lat_lon_t working_latitude;
    set_timelocation_lat_lon_t working_longitude;
} set_timelocation_state_t;

void set_timelocation_face_setup(uint8_t watch_face_index, void ** context_ptr);
void set_timelocation_face_activate(void *context);
bool set_timelocation_face_loop(movement_event_t event, void *context);
void set_timelocation_face_resign(void *context);

#define set_timelocation_face ((const watch_face_t){ \
    set_timelocation_face_setup, \
    set_timelocation_face_activate, \
    set_timelocation_face_loop, \
    set_timelocation_face_resign, \
    NULL, \
})

#endif // SET_TIMELOCATION_FACE_H_
