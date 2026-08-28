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

#ifndef WADOKEI_FACE_H_
#define WADOKEI_FACE_H_

/*
 * WADOKEI (JAPANESE TWELVE-BRANCH CLOCK) FACE
 *
 * Displays time using the traditional Japanese unequal-hour system: the day/night
 * boundary is twilight (solar altitude -7°21'40", not the usual -6° civil twilight),
 * each half is split into the six zodiacal double-hours (十二支), and each of those
 * is further split into four quarters (一つ〜四つ).
 *
 * Reads/writes the shared location register ("location.u32", the same file
 * sunrise_sunset_face and moon_phase_face use). The location-entry UI below is copied
 * from sunrise_sunset_face.c's settings pages (digit editor for custom LCD), minus its
 * named-preset cycling, which this face doesn't need.
 *
 * Press ALARM (short) to toggle between two ways of naming the same current branch/quarter:
 *   0: traditional bell-count "koku" name (暁九つ/明六つ/朝五つ/昼九つ/暮六つ/夜四つ
 *      etc.), with "半" for the second half of each branch
 *   1: zodiacal branch (十二支) + quarter (一つ〜四つ)
 * Press ALARM (long) to enter/exit the latitude/longitude entry screen; while entering,
 * LIGHT moves to the next digit and ALARM (short) increments the current digit.
 */

#include "movement.h"

typedef struct {
    uint8_t sign: 1;
    uint8_t hundreds: 5;
    uint8_t tens: 5;
    uint8_t ones: 4;
    uint8_t tenths: 4;
    uint8_t hundredths: 4;
} wadokei_lat_lon_settings_t;

typedef struct {
    uint32_t span_start_unix;  // start of the current branch span (unix time)
    uint32_t span_end_unix;    // end of the current branch span (unix time); recompute when now >= this
    bool is_daytime;
    bool valid;                // false if no location set, or __sunriset__ can't resolve (polar day/night)
    uint8_t mode;               // 0 = koku bell-count display, 1 = 十二支 branch/quarter display

    uint8_t page;               // 0 = normal display, 1 = editing latitude, 2 = editing longitude
    uint8_t active_digit;
    bool location_changed;
    wadokei_lat_lon_settings_t working_latitude;
    wadokei_lat_lon_settings_t working_longitude;
} wadokei_state_t;

void wadokei_face_setup(uint8_t watch_face_index, void ** context_ptr);
void wadokei_face_activate(void *context);
bool wadokei_face_loop(movement_event_t event, void *context);
void wadokei_face_resign(void *context);

#define wadokei_face ((const watch_face_t){ \
    wadokei_face_setup, \
    wadokei_face_activate, \
    wadokei_face_loop, \
    wadokei_face_resign, \
    NULL, \
})

#endif // WADOKEI_FACE_H_
