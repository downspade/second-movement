/* SPDX-License-Identifier: MIT */

/*
 * MIT License
 *
 * Copyright (c) 2023 Jonas Termeau
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

#ifndef MINUTE_REPEATER_FACE_H_
#define MINUTE_REPEATER_FACE_H_

/*
 * MINUTE REPEATER face
 *
 * A hopefully useful complication for friendly neighbors in the dark.
 * Ported from joeycastillo/Sensor-Watch's repetition_minute_face to
 * Movement's current watch_face_t API (no more movement_settings_t
 * parameter, advise() replaces wants_background_task(), etc).
 *
 * Originating from 1676 from reverend and mechanician Edward Barlow, and
 * perfected in 1820 by neighbor Abraham Breguet, a minute repeater or
 * "repetition minute" is a complication in a mechanical watch or clock that
 * chimes the hours and often minutes at the press of a button. There are many
 * types of repeater, from the simple repeater which merely strikes the number
 * of hours, to the minute repeater which chimes the time down to the minute,
 * using separate tones for hours, quarter hours, and minutes. They originated
 * before widespread artificial illumination, to allow the time to be determined
 * in the dark, and were also used by the visually impaired.
 *
 * How to use it:
 *
 * Long press the LIGHT button to get an audible reading of the time:
 * 0..23 (1..12 if 24-hour format isn't enabled) low beep(s) for the hours
 * 0..3 low-high paired beeps for the quarters
 * 0..14 high pitched beep(s) for the remaining minutes
 *
 * Long press ALARM to toggle the hourly chime, same as clock_face.
 *
 * ~ Only in the darkness can you see the stars. - Martin Luther King ~
 */

#include "movement.h"

typedef struct {
    watch_date_time_t previous;
    uint8_t last_battery_check;
    uint8_t watch_face_index;
    bool time_signal_enabled;
    bool battery_low;
} minute_repeater_state_t;

void minute_repeater_face_setup(uint8_t watch_face_index, void ** context_ptr);
void minute_repeater_face_activate(void *context);
bool minute_repeater_face_loop(movement_event_t event, void *context);
void minute_repeater_face_resign(void *context);
movement_watch_face_advisory_t minute_repeater_face_advise(void *context);

#define minute_repeater_face ((const watch_face_t) { \
    minute_repeater_face_setup, \
    minute_repeater_face_activate, \
    minute_repeater_face_loop, \
    minute_repeater_face_resign, \
    minute_repeater_face_advise, \
})

#endif // MINUTE_REPEATER_FACE_H_
