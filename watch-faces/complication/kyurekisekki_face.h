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

// Japanese lunisolar calendar (month/day/leap) and 六曜 on the lunar side, 24-sekki (二十四節気) solar terms on the other; long-press Alarm toggles between them, Alarm/Light step forward/back, long-press Light illuminates.

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
