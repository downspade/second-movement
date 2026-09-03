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

#ifndef KYUREKI_FACE_H_
#define KYUREKI_FACE_H_

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
 * solar day-of-month instead -- see kyureki_face.c's own comment on that).
 *
 * Press Alarm to step forward one day at a time (browsing tomorrow's, the day after's, etc.
 * rokuyo and lunar date), Light to step back a day; long-press Alarm to jump back to today,
 * long-press Light to illuminate the display (a plain tap of Light doesn't -- it's claimed by
 * the day-back step instead). The offset isn't persisted -- leaving the face (or falling
 * asleep) resets it back to today, same as moon_phase_ascii's own day-offset browsing.
 */

#include "movement.h"

typedef struct {
    watch_date_time_t last_computed_date; // only recompute when the (offset) local date changes
    uint8_t month_number;
    bool is_leap;
    uint8_t day_of_month;
    bool valid; // false if the current date falls outside the table's covered range
    int32_t offset_days; // Alarm-button browsing offset from today; reset on resign/sleep
} kyureki_state_t;

void kyureki_face_setup(uint8_t watch_face_index, void ** context_ptr);
void kyureki_face_activate(void *context);
bool kyureki_face_loop(movement_event_t event, void *context);
void kyureki_face_resign(void *context);

#define kyureki_face ((const watch_face_t){ \
    kyureki_face_setup, \
    kyureki_face_activate, \
    kyureki_face_loop, \
    kyureki_face_resign, \
    NULL, \
})

#endif // KYUREKI_FACE_H_
