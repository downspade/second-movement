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

#ifndef KYUREKISEKKI_FACE_H_
#define KYUREKISEKKI_FACE_H_

/*
 * KYUREKI (JAPANESE LUNISOLAR CALENDAR) FACE
 *
 * Displays the current date in the traditional Japanese lunisolar calendar
 * (month, day, and whether the current month is a leap month), looked up from
 * a precomputed table (see kyureki_table_data.h) covering KYUREKI_TABLE_BASE_YEAR
 * through KYUREKI_TABLE_BASE_YEAR + KYUREKI_TABLE_NUM_YEARS - 1.
 *
 * No location needed; this face just reads the current local date.
 *
 * TOP (5 chars, custom LCD): 六曜 (rokuyo, the traditional 6-day lucky/unlucky cycle),
 * derived from the lunar month and day (index = (month + day - 2) % 6; leap months use the
 * same index as their preceding month, per convention).
 * BOTTOM (6 chars): lunar month.day, both right-aligned with no leading zero (e.g. " 8. 5",
 * "12.28"), decimal point lit between them; the last 2 characters show "Ud" when the current
 * month is a leap month, else blank (or, on custom while browsing a non-today date, the
 * solar day-of-month instead -- see kyurekisekki_face.c's own comment on that).
 *
 * Press Alarm to step forward one day at a time (browsing tomorrow's, the day after's, etc.
 * rokuyo and lunar date), Light to step back a day; long-press Alarm to switch into 24-sekki
 * mode (see below), long-press Light to illuminate the display (a plain tap of Light doesn't
 * -- it's claimed by the day-back step instead). The offset isn't persisted -- leaving the
 * face (or falling asleep) resets it back to today, same as moon_phase_ascii's own day-offset
 * browsing.
 *
 * 24-SEKKI (二十四節気) MODE
 *
 * Long-press Alarm again to switch back to the lunar/rokuyo display above. Entering the
 * mode shows whichever term -- the one just passed, or the one still to come -- is nearest
 * to today; from there, Alarm/Light step forward/back through the 24 solar terms themselves
 * (not by day), and long-press Light still illuminates. The term names come from
 * kyurekisekki_names_data.h.
 *
 * Term dates aren't looked up from a table (unlike the lunar calendar above): the 24 terms
 * are just the 24 points where the sun's ecliptic longitude is a multiple of 15 degrees
 * (term 0, 立春, at 315 degrees), so each one's date is solved for directly using the same
 * low-precision solar position formula sunrise_sunset_face already relies on (see
 * sun_ecliptic_longitude() in lib/sunriset). As with the lunar table, predictions far into
 * the future inherit that model's accuracy limits, but there's no hard cutoff year the way
 * the lunar table has.
 *
 * Unlike the lunar display above, this needs the wearer's actual UTC offset (not just their
 * local calendar day): a term's exact crossing instant is a real moment in time, and which
 * calendar day it's considered to fall on can differ by the wearer's timezone. movement's
 * configured timezone offset is applied both when reading "now" and when converting the
 * solved crossing time back to a displayed date, so the date shown is the term's date in
 * the wearer's own local calendar, not UTC's.
 *
 * TOP (5 chars, custom LCD): the term name (sekki_names_5).
 * BOTTOM (6 chars, custom LCD): the term's Gregorian date as month/day/2-digit-year, right
 * -aligned in pairs ("M D YY"), decimal point lit between month and day (same convention as
 * the lunar month.day display above) -- e.g. September 23 2026 reads " 9.2326".
 * Classic LCD has no decimal point and no room for both name and date at once, so it uses
 * BOTTOM only, alternating between the 6-char name (sekki_names_6) and the same digits-only
 * date every 2 seconds.
 */

#include "movement.h"

typedef struct {
    watch_date_time_t last_computed_date; // only recompute when the (offset) local date changes
    uint8_t month_number;
    bool is_leap;
    uint8_t day_of_month;
    bool valid; // false if the current date falls outside the table's covered range
    int32_t offset_days; // Alarm-button browsing offset from today; reset on resign/sleep

    bool sekki_mode; // false = lunar/rokuyo display above; true = 24-sekki display
    int8_t sekki_offset; // Alarm/Light browsing offset, in terms (not days), from the term nearest to today; reset on mode switch/resign/sleep
    watch_date_time_t sekki_computed_date; // the (day-level) "now" the cached term below was computed from
    int8_t sekki_computed_offset; // the sekki_offset the cached term below was computed from
    uint8_t sekki_term_index; // 0-23, cached index into sekki_names_5/6 of the displayed term
    watch_date_time_t sekki_term_date; // cached Gregorian date of that term
} kyureki_state_t;

void kyurekisekki_face_setup(uint8_t watch_face_index, void ** context_ptr);
void kyurekisekki_face_activate(void *context);
bool kyurekisekki_face_loop(movement_event_t event, void *context);
void kyurekisekki_face_resign(void *context);

#define kyurekisekki_face ((const watch_face_t){ \
    kyurekisekki_face_setup, \
    kyurekisekki_face_activate, \
    kyurekisekki_face_loop, \
    kyurekisekki_face_resign, \
    NULL, \
})

#endif // KYUREKISEKKI_FACE_H_
