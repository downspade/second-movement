/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
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

#ifndef MOON_PHASE_ASCII_FACE_H_
#define MOON_PHASE_ASCII_FACE_H_

/*
 * MOON PHASE ASCII face (custom LCD only)
 *
 * Renamed and simplified from moon_phase_face: that face's text-label
 * display mode (a "1stQtr"/"FULL"/etc string plus a classic-LCD pixel
 * graphic) has been removed, along with the button chord that used to
 * toggle into this one -- this face only ever shows the pictorial
 * representation below.
 *
 * Top row: the moon's age in days -- a variable-width integer part with no
 * leading zero, plus a fixed single fractional digit, right-aligned with a
 * blank trailing character (e.g. an age of 15.2 days displays as " 152 ",
 * an age of 5.2 as "  52 ").
 *
 * Hours/minutes positions: a 4-character bar that sweeps as the moon waxes
 * and wanes -- "[   " a waxing crescent, "[=  " first quarter, "[== " waxing
 * gibbous, "[==]" full, " ==]" waning gibbous, "  =]" last quarter, "   ]"
 * a waning crescent, and blank for new. Waxes from the right for the
 * (default) northern hemisphere, mirrored for the southern hemisphere.
 *
 * Seconds position: the day of the month.
 *
 * Press the Alarm button repeatedly to move forward in time and watch the
 * moon phase advance; press Light to move back. Holding Light illuminates
 * the display.
 *
 * Hold Alarm to jump into the eclipse calendar instead of today's date (see
 * lunar_eclipses in the .c file) -- a separate mode, independent of the day
 * offset above, for browsing the eclipse table directly:
 *
 *  - Top-left: the eclipse's rate (magnitude_pct; over 100% uses all 3 digit
 *    slots, see the comment at that sprintf in the .c file).
 *  - Top-right: the last 2 digits of the year.
 *  - Hours/minutes/seconds: month, day, and hour, all local to the wearer
 *    (the table stores UTC).
 *  - The sleep indicator (the same crescent-moon icon shown in low energy
 *    mode) lights up if the eclipse happens at night at the saved location
 *    -- i.e. it's actually visible there.
 *
 * Alarm/Light single-press step to the next/previous eclipse; long-pressing
 * Alarm again returns to today's date and the ordinary moon phase display.
 *
 * This face checks whether the displayed day is a lunar eclipse, using a table of eclipse
 * dates and magnitudes covering the full 150 years this was meant to grow to, 2020-2170 (see
 * lunar_eclipses in the .c file for sourcing and a caveat about penumbral eclipses' displayed
 * magnitude). Unlike a solar eclipse, a lunar eclipse is a location-independent fact (the
 * Moon passes through Earth's shadow at the same moment for every observer), so this doesn't
 * need a location to be set -- only the calendar mode's "is it visible from here" indicator
 * below does. Eclipses only happen at full moon, so on an eclipse day the "[==]" bar is
 * otherwise unchanged, except for one of two things:
 *
 *  - Partial/total eclipse: part of the bar blinks instead of staying lit,
 *    in 6 magnitude-scaled steps -- from just the '[' bracket cell at the
 *    lowest magnitudes, through the hour digit turning into a second '['
 *    (by way of a "vertical line only" half-step), then the minute digit
 *    similarly turning into a second ']', up to all 4 cells (a comfortably
 *    total eclipse) at the highest.
 *  - Penumbral eclipse (no umbral magnitude, so no bar shape to size):
 *    the colon blinks instead.
 */

#include "movement.h"

typedef struct {
    uint32_t offset;
    bool southern_hemisphere;
    bool location_set;
    uint8_t eclipse_level; // 0 = no partial/total eclipse today (see _eclipse_bar_mask), else 1-6
    bool blink_on; // current blink phase while eclipse_level > 0, toggles every second
    bool calendar_mode; // browsing the eclipse calendar (Alarm long press) instead of the moon phase
    size_t calendar_index; // which entry of lunar_eclipses[] calendar mode is showing
} moon_phase_ascii_state_t;

void moon_phase_ascii_face_setup(uint8_t watch_face_index, void ** context_ptr);
void moon_phase_ascii_face_activate(void *context);
bool moon_phase_ascii_face_loop(movement_event_t event, void *context);
void moon_phase_ascii_face_resign(void *context);

#define moon_phase_ascii_face ((const watch_face_t){ \
    moon_phase_ascii_face_setup, \
    moon_phase_ascii_face_activate, \
    moon_phase_ascii_face_loop, \
    moon_phase_ascii_face_resign, \
    NULL, \
})

#endif // MOON_PHASE_ASCII_FACE_H_
