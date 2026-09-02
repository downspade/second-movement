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

#pragma once

/*
 * COUNTER TERM face
 *
 * A plain tally counter paired with an elapsed-days-since-reset display.
 *
 * Short-press ALARM to increment the counter (wraps from 9999 back to 0).
 * Long-press ALARM to reset the counter to 0 and record today's date as day one.
 *
 * Top row: days elapsed since the recorded date (e.g. "  0dy", " 42dy"), by calendar date --
 * the day of the reset itself shows 0dy, and this advances at local midnight.
 * Main line, hours+minutes only (4 digits): the counter.
 */

#include "movement.h"

typedef struct {
    uint16_t counter;
    uint16_t start_year;
    uint8_t start_month;
    uint8_t start_day;
} counter_term_state_t;

void counter_term_face_setup(uint8_t watch_face_index, void ** context_ptr);
void counter_term_face_activate(void *context);
bool counter_term_face_loop(movement_event_t event, void *context);
void counter_term_face_resign(void *context);

#define counter_term_face ((const watch_face_t){ \
    counter_term_face_setup, \
    counter_term_face_activate, \
    counter_term_face_loop, \
    counter_term_face_resign, \
    NULL, \
})
