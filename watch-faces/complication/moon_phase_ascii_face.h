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
 * MOON PHASE ASCII face
 *
 * Renamed and simplified from moon_phase_face: that face's text-label
 * display mode (a "1stQtr"/"FULL"/etc string plus a classic-LCD pixel
 * graphic) has been removed, along with the button chord that used to
 * toggle into this one -- this face only ever shows the pictorial
 * representation below.
 *
 * Top-left: the moon's age in days -- a variable-width integer part with no
 * leading zero, plus a fixed single fractional digit, right-aligned to 3
 * characters (e.g. an age of 15.2 days displays as "152", an age of 5.2 as
 * " 52").
 *
 * Hours/minutes positions: a 4-character bar that sweeps as the moon waxes
 * and wanes, 10 states per cycle -- blank for new, "   |" a hair-thin sliver
 * just starting to wax, "[   " a waxing crescent, "[=  " first quarter,
 * "[== " waxing gibbous, "[==]" full, " ==]" waning gibbous, "  =]" last
 * quarter, "   ]" a waning crescent, and "|   " a sliver having just
 * finished waning, back to blank for new. Waxes from the right for the
 * (default) northern hemisphere, mirrored for the southern hemisphere.
 *
 * Seconds position: the day of the month.
 *
 * The PM indicator (repurposed here -- this face never shows a 12-hour time, so it's free;
 * "P.M." as in "Present Moon") lights up whenever the Moon is actually above the horizon
 * right now at the saved location (see set_location_face) -- e.g. it's on for a first-quarter
 * moon in the afternoon/evening, off for a waning-crescent moon in the afternoon. This is a
 * rough estimate (today's sunrise/set shifted by the Moon's current elongation from the Sun --
 * see _moon_visible_now in the .c file), not a full moon-position calculation, and stays off
 * if no location has been saved. It tracks whatever day is being displayed, so it also updates
 * as you step through days with Alarm/Light below -- including on an eclipse day, where it
 * still just means "is the Moon up now" rather than "was this eclipse visible" (that's a
 * separate question, answered by the eclipse calendar's own indicator further down instead).
 * It keeps updating every hour even while the watch is asleep (see the EVENT_LOW_ENERGY_UPDATE
 * handler in the .c file), same as while awake -- meanwhile the sleep indicator itself is left
 * to mean what it usually does (lit steadily while the watch is in low energy mode).
 *
 * Press the Alarm button repeatedly to move forward in time and watch the
 * moon phase advance; press Light to move back. Holding Light illuminates
 * the display.
 *
 * Hold Alarm to jump into the eclipse calendar instead of today's date (see
 * lunar_eclipses in the .c file) -- a separate mode, independent of the day
 * offset above, for browsing the eclipse table directly:
 *
 *  - Top-left: the eclipse's rate -- magnitude_pct on custom (over 100% uses
 *    all 3 digit slots, see the comment at that sprintf in the .c file), or a
 *    TO(tal)/PA(rtial)/PE(numbral) type code on classic (which has no room
 *    for a 3-digit percentage -- see the comment in the .c file).
 *  - Top-right: the peak hour.
 *  - Hours/minutes: month and day, all local to the wearer (the table stores
 *    UTC).
 *  - Seconds: the last 2 digits of the year (swapped down here from the more
 *    obvious top-right spot -- see the comment in the .c file for why).
 *  - The PM ("Present Moon") indicator lights up if the eclipse happens at
 *    night at the saved location -- i.e. it's actually visible there.
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
 *  - Partial/total eclipse with a nonzero magnitude: the bar blinks once a second between
 *    the ordinary full "[==]" and a plain moon-phase-style shape sized to the fraction NOT
 *    covered -- e.g. 50% magnitude blinks between "[==]" and "[=  " (a plain half moon), a
 *    total eclipse blinks between "[==]" and blank (fully covered).
 *  - Penumbral eclipse (no umbral magnitude, so no fraction to size a shape by), or a
 *    partial/total eclipse whose magnitude happens to be 0%: the bar stays steadily "[==]"
 *    and the colon blinks instead.
 */

#include "movement.h"

typedef struct {
    uint32_t offset;
    bool southern_hemisphere;
    bool location_set;
    uint8_t eclipse_bar_magnitude; // 0 = bar doesn't blink today, else the magnitude (1-100+) sizing the blink's alternate shape
    bool blink_on; // current blink phase while eclipse_bar_magnitude > 0, toggles every second
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
