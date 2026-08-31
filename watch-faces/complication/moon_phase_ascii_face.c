/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
 *
 * Based on Phase of Moon App for Tidbyt
 * https://github.com/tidbyt/community/blob/main/apps/phaseofmoon/phase_of_moon.star
 * Copyright (c) 2022 Alan Fleming
 *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "moon_phase_ascii_face.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "filesystem.h"
#include "sunriset.h"

#define LUNAR_DAYS 29.53058770576
#define NUM_PHASES 10

// Meeus degrees-to-radians conversion, same literal PI sunriset.c (already linked into this
// face for twilight/eclipse-visibility math) uses -- not relying on math.h's M_PI, which
// isn't guaranteed by plain C11.
#define MOON_DEGRAD (3.1415926535897932384 / 180.0)

// 12 breakpoints for the 11 phase_index windows (0..10) below. Identical to the original
// 9-window breakpoints except the waxing- and waning-crescent windows ([1, 6.38...] and
// [23.14..., 28.53...]) each give up their outermost 1-day sliver to a new adjacent sliver
// window ([1,2] and [27.53...,28.53...]) -- the quarter/full windows (already narrow, ~2
// days centered on the exact moment) are untouched.
static const float phase_changes[] = {0, 1, 2, 6.38264692644, 8.38264692644, 13.76529385288, 15.76529385288, 21.14794077932, 23.14794077932, 27.53058770576, 28.53058770576, 29.53058770576};

// ASCII-art moon phase bars: 4 cells drawn across WATCH_POSITION_HOURS +
// WATCH_POSITION_MINUTES (raw positions 4-7). '[' is a moon's leading
// (rounded) edge, ']' its trailing edge, '=' a fully-lit cell in between
// (see _display_bar_segment: no character in the font combines exactly the
// top+bottom segments this needs), '|' a hair-thin sliver -- just the near
// vertical edge of the outermost cell, thinner than '['/']' since it has no
// top/bottom cap (see _draw_phase_bar: the font's own '|' draws both
// verticals, so this needs raw segments too) -- and space is dark. Written
// here waxing-from-the-left (cell count = phase_index, 0..5; full is all
// four cells; waning empties back out from the left, leaving the lit
// remainder anchored to the right, cell count = 10 - phase_index) -- see
// _update, which mirrors this to waxing-from-the-right for the (northern-
// hemisphere) default case. Indices 0 and 10 are both "new" (the actual
// instant of new moon isn't otherwise a displayed state); every other index
// is a distinct phase, giving 10 displayed states total per cycle.
static const char *const ascii_art_moon[NUM_PHASES + 1] = {
    "    ", // 0: new
    "|   ", // 1: sliver, just waxing
    "[   ", // 2: waxing crescent
    "[=  ", // 3: first quarter (half)
    "[== ", // 4: waxing gibbous
    "[==]", // 5: full
    " ==]", // 6: waning gibbous
    "  =]", // 7: last quarter (half)
    "   ]", // 8: waning crescent
    "   |", // 9: sliver, just waned
    "    ", // 10: new
};

// Returns whether a location has been set (see set_location_face, or sunrise_sunset_face's
// own location-entry UI); if so, also updates state->southern_hemisphere.
static bool _read_location(moon_phase_ascii_state_t *state) {
    movement_location_t location = {0};
    bool have_location = filesystem_read_file("location.u32", (char *) &location.reg, sizeof(movement_location_t));
    if (have_location) {
        state->southern_hemisphere = (int16_t)location.bit.latitude < 0;
    }
    return have_location;
}

// Lunar eclipse dates and the UTC time of greatest eclipse, covering the full 150 years this
// was meant to grow to: 2020-2170. magnitude_pct is x100, rounded: the umbral magnitude for
// partial/total eclipses, the penumbral magnitude for penumbral eclipses (these are two
// different, independently-computed numbers -- the umbral magnitude of a penumbral eclipse is
// always negative, since the moon never reaches the umbra, so it can't just be reused). Source:
// NASA GSFC's Five Millennium Canon of Lunar Eclipses (eclipse.gsfc.nasa.gov/LEcat5/
// LE2001-2100.html for 2001-2100, LE2101-2200.html for 2101-2170) for dates and magnitudes,
// cross-checked throughout against Wikipedia/EclipseWise (agreement within ~0.002 magnitude and
// under a minute of time in every spot check). The 223 partial/total rows were fetched a batch
// of table rows at a time; the 129 penumbral rows' magnitudes were fetched one eclipse at a time
// from their individual EclipseWise pages instead, after a batched fetch attempt for those
// produced values that didn't hold up under cross-checking (conflating penumbral eclipses with
// nearby partial ones, and in a couple of cases returning magnitudes over 1.9, outside what's
// physically possible) -- individual per-eclipse fetches were slower but didn't have that
// problem. Written down rather than computed on-device: predicting eclipses requires modeling
// the Moon's ~18.6-year nodal precession against its phase, which is a much harder problem than
// the plain synodic-month approximation the rest of this file uses for phase, and -- per this
// project's usual policy (see wadokei_kyureki_notes.md's lunar calendar table) -- verified
// external data beats an approximation that could silently drift.
//
// year is years since WATCH_RTC_REFERENCE_YEAR (2020), matching date_time.unit.year directly.
// Hour/minute have seconds truncated (not rounded) from the source, which is plenty of
// precision for a display that only ever shows the hour.
typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;   // UTC, time of greatest eclipse
    uint8_t minute; // UTC
    uint8_t magnitude_pct;
    bool penumbral;
} lunar_eclipse_t;

static const lunar_eclipse_t lunar_eclipses[] = {
    {0, 1, 10, 19, 11, 90, true},        // 2020-01-10 penumbral
    {0, 6, 5, 19, 26, 57, true},         // 2020-06-05 penumbral
    {0, 7, 5, 4, 31, 36, true},          // 2020-07-05 penumbral
    {0, 11, 30, 9, 44, 83, true},        // 2020-11-30 penumbral
    {1, 5, 26, 11, 19, 101, false},      // 2021-05-26 total
    {1, 11, 19, 9, 4, 97, false},        // 2021-11-19 partial
    {2, 5, 16, 4, 12, 141, false},       // 2022-05-16 total
    {2, 11, 8, 11, 0, 136, false},       // 2022-11-08 total
    {3, 5, 5, 17, 24, 97, true},          // 2023-05-05 penumbral
    {3, 10, 28, 20, 15, 12, false},      // 2023-10-28 partial
    {4, 3, 25, 7, 13, 96, true},         // 2024-03-25 penumbral
    {4, 9, 18, 2, 45, 8, false},         // 2024-09-18 partial
    {5, 3, 14, 6, 59, 118, false},       // 2025-03-14 total
    {5, 9, 7, 18, 11, 136, false},       // 2025-09-07 total
    {6, 3, 3, 11, 34, 115, false},       // 2026-03-03 total
    {6, 8, 28, 4, 14, 93, false},        // 2026-08-28 partial
    {7, 2, 20, 23, 14, 93, true},         // 2027-02-20 penumbral
    {7, 7, 18, 16, 4, 0, true},        // 2027-07-18 penumbral
    {7, 8, 17, 7, 14, 55, true},         // 2027-08-17 penumbral
    {8, 1, 12, 4, 14, 7, false},         // 2028-01-12 partial
    {8, 7, 6, 18, 20, 39, false},        // 2028-07-06 partial
    {8, 12, 31, 16, 52, 125, false},     // 2028-12-31 total
    {9, 6, 26, 3, 23, 184, false},       // 2029-06-26 total
    {9, 12, 20, 22, 43, 112, false},     // 2029-12-20 total
    {10, 6, 15, 18, 34, 50, false},      // 2030-06-15 partial
    {10, 12, 9, 22, 28, 94, true},       // 2030-12-09 penumbral
    {11, 5, 7, 3, 52, 88, true},          // 2031-05-07 penumbral
    {11, 6, 5, 11, 45, 13, true},        // 2031-06-05 penumbral
    {11, 10, 30, 7, 46, 72, true},       // 2031-10-30 penumbral
    {12, 4, 25, 15, 14, 119, false},     // 2032-04-25 total
    {12, 10, 18, 19, 3, 110, false},     // 2032-10-18 total
    {13, 4, 14, 19, 13, 109, false},     // 2033-04-14 total
    {13, 10, 8, 10, 56, 135, false},     // 2033-10-08 total
    {14, 4, 3, 19, 6, 86, true},         // 2034-04-03 penumbral
    {14, 9, 28, 2, 47, 1, false},        // 2034-09-28 partial
    {15, 2, 22, 9, 6, 97, true},          // 2035-02-22 penumbral
    {15, 8, 19, 1, 12, 10, false},       // 2035-08-19 partial
    {16, 2, 11, 22, 13, 130, false},     // 2036-02-11 total
    {16, 8, 7, 2, 52, 145, false},       // 2036-08-07 total
    {17, 1, 31, 14, 1, 121, false},      // 2037-01-31 total
    {17, 7, 27, 4, 9, 81, false},        // 2037-07-27 partial
    {18, 1, 21, 3, 49, 90, true},        // 2038-01-21 penumbral
    {18, 6, 17, 2, 45, 44, true},        // 2038-06-17 penumbral
    {18, 7, 16, 11, 35, 50, true},       // 2038-07-16 penumbral
    {18, 12, 11, 17, 45, 81, true},      // 2038-12-11 penumbral
    {19, 6, 6, 18, 54, 88, false},       // 2039-06-06 partial
    {19, 11, 30, 16, 56, 94, false},     // 2039-11-30 partial
    {20, 5, 26, 11, 46, 153, false},     // 2040-05-26 total
    {20, 11, 18, 19, 4, 140, false},     // 2040-11-18 total
    {21, 5, 16, 0, 43, 6, false},        // 2041-05-16 partial
    {21, 11, 8, 4, 35, 17, false},       // 2041-11-08 partial
    {22, 4, 5, 14, 30, 87, true},        // 2042-04-05 penumbral
    {22, 9, 29, 10, 45, 95, true},        // 2042-09-29 penumbral
    {23, 3, 25, 14, 32, 111, false},     // 2043-03-25 total
    {23, 9, 19, 1, 51, 126, false},      // 2043-09-19 total
    {24, 3, 13, 19, 38, 120, false},     // 2044-03-13 total
    {24, 9, 7, 11, 20, 105, false},      // 2044-09-07 total
    {25, 3, 3, 7, 43, 96, true},          // 2045-03-03 penumbral
    {25, 8, 27, 13, 54, 68, true},       // 2045-08-27 penumbral
    {26, 1, 22, 13, 2, 5, false},        // 2046-01-22 partial
    {26, 7, 18, 1, 6, 25, false},        // 2046-07-18 partial
    {27, 1, 12, 1, 26, 123, false},      // 2047-01-12 total
    {27, 7, 7, 10, 35, 175, false},      // 2047-07-07 total
    {28, 1, 1, 6, 53, 113, false},       // 2048-01-01 total
    {28, 6, 26, 2, 2, 64, false},        // 2048-06-26 partial
    {28, 12, 20, 6, 27, 96, true},       // 2048-12-20 penumbral
    {29, 5, 17, 11, 26, 77, true},       // 2049-05-17 penumbral
    {29, 6, 15, 19, 14, 25, true},       // 2049-06-15 penumbral
    {29, 11, 9, 15, 52, 68, true},       // 2049-11-09 penumbral
    {30, 5, 6, 22, 32, 108, false},      // 2050-05-06 total
    {30, 10, 30, 3, 21, 105, false},     // 2050-10-30 total
    {31, 4, 26, 2, 16, 120, false},      // 2051-04-26 total
    {31, 10, 19, 19, 11, 141, false},    // 2051-10-19 total
    {32, 4, 14, 2, 18, 95, true},        // 2052-04-14 penumbral
    {32, 10, 8, 10, 45, 8, false},       // 2052-10-08 partial
    {33, 3, 4, 17, 22, 93, true},         // 2053-03-04 penumbral
    {33, 8, 29, 8, 5, 102, true},          // 2053-08-29 penumbral
    {34, 2, 22, 6, 51, 128, false},      // 2054-02-22 total
    {34, 8, 18, 9, 26, 131, false},      // 2054-08-18 total
    {35, 2, 11, 22, 46, 122, false},     // 2055-02-11 total
    {35, 8, 7, 10, 53, 96, false},       // 2055-08-07 partial
    {36, 2, 1, 12, 26, 91, true},        // 2056-02-01 penumbral
    {36, 6, 27, 10, 3, 32, true},        // 2056-06-27 penumbral
    {36, 7, 26, 18, 43, 64, true},       // 2056-07-26 penumbral
    {36, 12, 22, 1, 48, 79, true},       // 2056-12-22 penumbral
    {37, 6, 17, 2, 26, 76, false},       // 2057-06-17 partial
    {37, 12, 11, 0, 53, 92, false},      // 2057-12-11 partial
    {38, 6, 6, 19, 15, 166, false},      // 2058-06-06 total
    {38, 11, 30, 3, 16, 143, false},     // 2058-11-30 total
    {39, 5, 27, 7, 55, 18, false},       // 2059-05-27 partial
    {39, 11, 19, 13, 1, 21, false},      // 2059-11-19 partial
    {40, 4, 15, 21, 37, 77, true},       // 2060-04-15 penumbral
    {40, 10, 9, 18, 53, 88, true},        // 2060-10-09 penumbral
    {40, 11, 8, 4, 4, 3, true},         // 2060-11-08 penumbral
    {41, 4, 4, 21, 54, 103, false},      // 2061-04-04 total
    {41, 9, 29, 9, 38, 116, false},      // 2061-09-29 total
    {42, 3, 25, 3, 33, 127, false},      // 2062-03-25 total
    {42, 9, 18, 18, 34, 115, false},     // 2062-09-18 total
    {43, 3, 14, 16, 5, 3, false},        // 2063-03-14 partial
    {43, 9, 7, 20, 41, 81, true},        // 2063-09-07 penumbral
    {44, 2, 2, 21, 48, 4, false},        // 2064-02-02 partial
    {44, 7, 28, 7, 52, 10, false},       // 2064-07-28 partial
    {45, 1, 22, 9, 58, 122, false},      // 2065-01-22 total
    {45, 7, 17, 17, 48, 161, false},     // 2065-07-17 total
    {46, 1, 11, 15, 4, 114, false},      // 2066-01-11 total
    {46, 7, 7, 9, 30, 78, false},        // 2066-07-07 partial
    {46, 12, 31, 14, 30, 98, true},      // 2066-12-31 penumbral
    {47, 5, 28, 18, 56, 64, true},       // 2067-05-28 penumbral
    {47, 6, 27, 2, 41, 38, true},        // 2067-06-27 penumbral
    {47, 11, 21, 0, 4, 66, true},        // 2067-11-21 penumbral
    {48, 5, 17, 5, 42, 95, false},       // 2068-05-17 partial
    {48, 11, 9, 11, 47, 101, false},     // 2068-11-09 total
    {49, 5, 6, 9, 9, 132, false},        // 2069-05-06 total
    {49, 10, 30, 3, 35, 146, false},     // 2069-10-30 total
    {50, 4, 25, 9, 21, 105, true},         // 2070-04-25 penumbral
    {50, 10, 19, 18, 51, 14, false},     // 2070-10-19 partial
    {51, 3, 16, 1, 31, 89, true},        // 2071-03-16 penumbral
    {51, 9, 9, 15, 5, 90, true},         // 2071-09-09 penumbral
    {52, 3, 4, 15, 23, 124, false},      // 2072-03-04 total
    {52, 8, 28, 16, 5, 117, false},      // 2072-08-28 total
    {53, 2, 22, 7, 24, 125, false},      // 2073-02-22 total
    {53, 8, 17, 17, 42, 110, false},     // 2073-08-17 total
    {54, 2, 11, 20, 55, 92, true},       // 2074-02-11 penumbral
    {54, 7, 8, 17, 21, 19, true},        // 2074-07-08 penumbral
    {54, 8, 7, 1, 56, 78, true},         // 2074-08-07 penumbral
    {55, 1, 2, 9, 55, 77, true},         // 2075-01-02 penumbral
    {55, 6, 28, 9, 55, 62, false},       // 2075-06-28 partial
    {55, 12, 22, 8, 55, 90, false},      // 2075-12-22 partial
    {56, 6, 17, 2, 39, 179, false},      // 2076-06-17 total
    {56, 12, 10, 11, 34, 145, false},    // 2076-12-10 total
    {57, 6, 6, 14, 59, 31, false},       // 2077-06-06 partial
    {57, 11, 29, 21, 35, 24, false},     // 2077-11-29 partial
    {58, 4, 27, 4, 35, 66, true},        // 2078-04-27 penumbral
    {58, 10, 21, 3, 8, 82, true},        // 2078-10-21 penumbral
    {58, 11, 19, 12, 40, 6, true},      // 2078-11-19 penumbral
    {59, 4, 16, 5, 10, 95, false},       // 2079-04-16 partial
    {59, 10, 10, 17, 30, 108, false},    // 2079-10-10 total
    {60, 4, 4, 11, 23, 135, false},      // 2080-04-04 total
    {60, 9, 29, 1, 52, 124, false},      // 2080-09-29 total
    {61, 3, 25, 0, 22, 10, false},       // 2081-03-25 partial
    {61, 9, 18, 3, 35, 93, true},        // 2081-09-18 penumbral
    {62, 2, 13, 6, 29, 1, false},        // 2082-02-13 partial
    {62, 8, 8, 14, 46, 100, true},         // 2082-08-08 penumbral
    {63, 2, 2, 18, 26, 121, false},      // 2083-02-02 total
    {63, 7, 29, 1, 5, 148, false},       // 2083-07-29 total
    {64, 1, 22, 23, 13, 115, false},     // 2084-01-22 total
    {64, 7, 17, 16, 58, 91, false},      // 2084-07-17 partial
    {65, 1, 10, 22, 32, 99, true},       // 2085-01-10 penumbral
    {65, 6, 8, 2, 17, 51, true},         // 2085-06-08 penumbral
    {65, 7, 7, 10, 4, 51, true},         // 2085-07-07 penumbral
    {65, 12, 1, 8, 25, 64, true},        // 2085-12-01 penumbral
    {66, 5, 28, 12, 43, 82, false},      // 2086-05-28 partial
    {66, 11, 20, 20, 19, 99, false},     // 2086-11-20 partial
    {67, 5, 17, 15, 55, 146, false},     // 2087-05-17 total
    {67, 11, 10, 12, 5, 150, false},     // 2087-11-10 total
    {68, 5, 5, 16, 16, 10, false},       // 2088-05-05 partial
    {68, 10, 30, 3, 3, 18, false},       // 2088-10-30 partial
    {69, 3, 26, 9, 34, 83, true},        // 2089-03-26 penumbral
    {69, 9, 19, 22, 11, 79, true},       // 2089-09-19 penumbral
    {70, 3, 15, 23, 48, 120, false},     // 2090-03-15 total
    {70, 9, 8, 22, 52, 104, false},      // 2090-09-08 total
    {71, 3, 5, 15, 58, 128, false},      // 2091-03-05 total
    {71, 8, 29, 0, 38, 124, false},      // 2091-08-29 total
    {72, 2, 23, 5, 20, 94, true},         // 2092-02-23 penumbral
    {72, 7, 19, 0, 41, 6, true},        // 2092-07-19 penumbral
    {72, 8, 17, 9, 13, 91, true},         // 2092-08-17 penumbral
    {73, 1, 12, 18, 0, 76, true},        // 2093-01-12 penumbral
    {73, 7, 8, 17, 24, 49, false},       // 2093-07-08 partial
    {74, 1, 1, 17, 0, 89, false},        // 2094-01-01 partial
    {74, 6, 28, 10, 1, 182, false},      // 2094-06-28 total
    {74, 12, 21, 19, 56, 146, false},    // 2094-12-21 total
    {75, 6, 17, 22, 0, 45, false},       // 2095-06-17 partial
    {75, 12, 11, 6, 15, 26, false},      // 2095-12-11 partial
    {76, 5, 7, 11, 24, 53, true},        // 2096-05-07 penumbral
    {76, 6, 6, 2, 43, 1, true},        // 2096-06-06 penumbral
    {76, 10, 31, 11, 30, 77, true},      // 2096-10-31 penumbral
    {76, 11, 29, 21, 22, 9, true},      // 2096-11-29 penumbral
    {77, 4, 26, 12, 18, 84, false},      // 2097-04-26 partial
    {77, 10, 21, 1, 30, 101, false},     // 2097-10-21 total
    {78, 4, 15, 19, 4, 144, false},      // 2098-04-15 total
    {78, 10, 10, 9, 19, 132, false},     // 2098-10-10 total
    {79, 4, 5, 8, 30, 17, false},        // 2099-04-05 partial
    {79, 9, 29, 10, 36, 104, true},        // 2099-09-29 penumbral
    {80, 2, 24, 15, 5, 97, true},         // 2100-02-24 penumbral
    {80, 8, 19, 21, 44, 87, true},       // 2100-08-19 penumbral
    {81, 2, 14, 2, 50, 118, false},      // 2101-02-14 total
    {81, 8, 9, 8, 25, 135, false},       // 2101-08-09 total
    {82, 2, 3, 7, 18, 117, false},       // 2102-02-03 total
    {82, 7, 30, 0, 29, 105, false},      // 2102-07-30 total
    {83, 1, 23, 6, 34, 101, true},         // 2103-01-23 penumbral
    {83, 6, 20, 9, 36, 37, true},        // 2103-06-20 penumbral
    {83, 7, 19, 17, 28, 63, true},       // 2103-07-19 penumbral
    {83, 12, 13, 16, 51, 63, true},      // 2103-12-13 penumbral
    {84, 6, 8, 19, 38, 67, false},       // 2104-06-08 partial
    {84, 12, 2, 4, 58, 97, false},       // 2104-12-02 partial
    {85, 5, 28, 22, 34, 160, false},     // 2105-05-28 total
    {85, 11, 21, 20, 42, 153, false},    // 2105-11-21 total
    {86, 5, 17, 23, 6, 23, false},       // 2106-05-17 partial
    {86, 11, 11, 11, 22, 22, false},     // 2106-11-11 partial
    {87, 4, 7, 17, 30, 77, true},        // 2107-04-07 penumbral
    {87, 5, 7, 4, 30, 1, true},        // 2107-05-07 penumbral
    {87, 10, 2, 5, 23, 69, true},        // 2107-10-02 penumbral
    {88, 3, 27, 8, 6, 115, false},       // 2108-03-27 total
    {88, 9, 20, 5, 47, 92, false},       // 2108-09-20 partial
    {89, 3, 17, 0, 22, 133, false},      // 2109-03-17 total
    {89, 9, 9, 7, 43, 136, false},       // 2109-09-09 total
    {90, 3, 6, 13, 37, 97, true},         // 2110-03-06 penumbral
    {90, 8, 29, 16, 38, 5, false},       // 2110-08-29 partial
    {91, 1, 25, 2, 3, 74, true},         // 2111-01-25 penumbral
    {91, 7, 21, 0, 53, 35, false},       // 2111-07-21 partial
    {92, 1, 14, 1, 6, 88, false},        // 2112-01-14 partial
    {92, 7, 9, 17, 19, 168, false},      // 2112-07-09 total
    {93, 1, 2, 4, 22, 147, false},       // 2113-01-02 total
    {93, 6, 29, 4, 55, 58, false},       // 2113-06-29 partial
    {93, 12, 22, 14, 58, 27, false},     // 2113-12-22 partial
    {94, 5, 19, 18, 7, 40, true},        // 2114-05-19 penumbral
    {94, 6, 18, 9, 17, 15, true},        // 2114-06-18 penumbral
    {94, 11, 12, 19, 59, 73, true},      // 2114-11-12 penumbral
    {94, 12, 12, 6, 9, 10, true},        // 2114-12-12 penumbral
    {95, 5, 8, 19, 21, 73, false},       // 2115-05-08 partial
    {95, 11, 2, 9, 36, 95, false},       // 2115-11-02 partial
    {96, 4, 27, 2, 41, 154, false},      // 2116-04-27 total
    {96, 10, 21, 16, 53, 139, false},    // 2116-10-21 total
    {97, 4, 16, 16, 32, 25, false},      // 2117-04-16 partial
    {97, 10, 10, 17, 47, 4, false},      // 2117-10-10 partial
    {98, 3, 7, 23, 33, 92, true},         // 2118-03-07 penumbral
    {98, 8, 31, 4, 51, 75, true},        // 2118-08-31 penumbral
    {99, 2, 25, 11, 5, 115, false},      // 2119-02-25 total
    {99, 8, 20, 15, 51, 122, false},     // 2119-08-20 total
    {100, 2, 14, 15, 17, 120, false},    // 2120-02-14 total
    {100, 8, 9, 8, 1, 118, false},       // 2120-08-09 total
    {101, 2, 2, 14, 32, 103, true},        // 2121-02-02 penumbral
    {101, 6, 30, 16, 49, 23, true},      // 2121-06-30 penumbral
    {101, 7, 30, 0, 52, 76, true},       // 2121-07-30 penumbral
    {101, 12, 24, 1, 22, 62, true},      // 2121-12-24 penumbral
    {102, 6, 20, 2, 27, 52, false},      // 2122-06-20 partial
    {102, 12, 13, 13, 42, 95, false},    // 2122-12-13 partial
    {103, 6, 9, 5, 6, 175, false},       // 2123-06-09 total
    {103, 12, 3, 5, 24, 155, false},     // 2123-12-03 total
    {104, 5, 28, 5, 50, 38, false},      // 2124-05-28 partial
    {104, 11, 21, 19, 47, 24, false},    // 2124-11-21 partial
    {105, 4, 18, 1, 18, 69, true},       // 2125-04-18 penumbral
    {105, 5, 17, 11, 46, 12, true},      // 2125-05-17 penumbral
    {105, 10, 12, 12, 43, 61, true},     // 2125-10-12 penumbral
    {106, 4, 7, 16, 17, 108, false},     // 2126-04-07 total
    {106, 10, 1, 12, 49, 82, false},     // 2126-10-01 partial
    {107, 3, 28, 8, 40, 138, false},     // 2127-03-28 total
    {107, 9, 20, 14, 56, 147, false},    // 2127-09-20 total
    {108, 3, 16, 21, 46, 101, true},       // 2128-03-16 penumbral
    {108, 9, 9, 0, 10, 16, false},       // 2128-09-09 partial
    {109, 2, 4, 10, 2, 71, true},        // 2129-02-04 penumbral
    {109, 7, 31, 8, 24, 22, false},      // 2129-07-31 partial
    {110, 1, 24, 9, 10, 86, false},      // 2130-01-24 partial
    {110, 7, 21, 0, 38, 154, false},     // 2130-07-21 total
    {111, 1, 13, 12, 49, 148, false},    // 2131-01-13 total
    {111, 7, 10, 11, 47, 73, false},     // 2131-07-10 partial
    {112, 1, 2, 23, 44, 28, false},      // 2132-01-02 partial
    {112, 5, 30, 0, 42, 26, true},       // 2132-05-30 penumbral
    {112, 6, 28, 15, 45, 30, true},      // 2132-06-28 penumbral
    {112, 11, 23, 4, 35, 70, true},      // 2132-11-23 penumbral
    {112, 12, 22, 14, 59, 11, true},     // 2132-12-22 penumbral
    {113, 5, 19, 2, 16, 61, false},      // 2133-05-19 partial
    {113, 11, 12, 17, 50, 90, false},    // 2133-11-12 partial
    {114, 5, 8, 10, 10, 165, false},     // 2134-05-08 total
    {114, 11, 2, 0, 34, 145, false},     // 2134-11-02 total
    {115, 4, 28, 0, 26, 35, false},      // 2135-04-28 partial
    {115, 10, 22, 1, 6, 12, false},      // 2135-10-22 partial
    {116, 3, 18, 7, 53, 87, true},       // 2136-03-18 penumbral
    {116, 4, 16, 17, 8, 4, true},       // 2136-04-16 penumbral
    {116, 9, 10, 12, 5, 65, true},       // 2136-09-10 penumbral
    {117, 3, 7, 19, 13, 111, false},     // 2137-03-07 total
    {117, 8, 30, 23, 24, 111, false},    // 2137-08-30 total
    {118, 2, 24, 23, 9, 123, false},     // 2138-02-24 total
    {118, 8, 20, 15, 38, 130, false},    // 2138-08-20 total
    {119, 2, 13, 22, 28, 106, true},       // 2139-02-13 penumbral
    {119, 7, 12, 0, 1, 9, true},        // 2139-07-12 penumbral
    {119, 8, 10, 8, 18, 89, true},        // 2139-08-10 penumbral
    {120, 1, 4, 9, 55, 62, true},        // 2140-01-04 penumbral
    {120, 6, 30, 9, 13, 37, false},      // 2140-06-30 partial
    {120, 12, 23, 22, 29, 95, false},    // 2140-12-23 partial
    {121, 6, 19, 11, 34, 174, false},    // 2141-06-19 total
    {121, 12, 13, 14, 10, 157, false},   // 2141-12-13 total
    {122, 6, 8, 12, 32, 52, false},      // 2142-06-08 partial
    {122, 12, 3, 4, 16, 26, false},      // 2142-12-03 partial
    {123, 4, 29, 9, 1, 60, true},        // 2143-04-29 penumbral
    {123, 5, 28, 18, 59, 25, true},      // 2143-05-28 penumbral
    {123, 10, 23, 20, 10, 53, true},     // 2143-10-23 penumbral
    {124, 4, 18, 0, 20, 100, false},     // 2144-04-18 total
    {124, 10, 11, 20, 2, 73, false},     // 2144-10-11 partial
    {125, 4, 7, 16, 48, 146, false},     // 2145-04-07 total
    {125, 9, 30, 22, 19, 156, false},    // 2145-09-30 total
    {126, 3, 28, 5, 44, 4, false},       // 2146-03-28 partial
    {126, 9, 20, 7, 51, 27, false},      // 2146-09-20 partial
    {127, 2, 15, 17, 57, 68, true},      // 2147-02-15 penumbral
    {127, 8, 11, 15, 57, 9, false},      // 2147-08-11 partial
    {127, 9, 9, 23, 11, 1, true},       // 2147-09-09 penumbral
    {128, 2, 4, 17, 13, 85, false},      // 2148-02-04 partial
    {128, 7, 31, 7, 56, 140, false},     // 2148-07-31 total
    {129, 1, 23, 21, 17, 150, false},    // 2149-01-23 total
    {129, 7, 20, 18, 38, 87, false},     // 2149-07-20 partial
    {130, 1, 13, 8, 31, 29, false},      // 2150-01-13 partial
    {130, 6, 10, 7, 14, 11, true},       // 2150-06-10 penumbral
    {130, 7, 9, 22, 13, 45, true},       // 2150-07-09 penumbral
    {130, 12, 4, 13, 14, 67, true},      // 2150-12-04 penumbral
    {131, 1, 2, 23, 51, 12, true},       // 2151-01-02 penumbral
    {131, 5, 30, 9, 9, 48, false},       // 2151-05-30 partial
    {131, 11, 24, 2, 8, 87, false},      // 2151-11-24 partial
    {132, 5, 18, 17, 35, 177, false},    // 2152-05-18 total
    {132, 11, 12, 8, 21, 150, false},    // 2152-11-12 total
    {133, 5, 8, 8, 14, 45, false},       // 2153-05-08 partial
    {133, 11, 1, 8, 34, 18, false},      // 2153-11-01 partial
    {134, 3, 29, 16, 5, 80, true},       // 2154-03-29 penumbral
    {134, 4, 28, 1, 4, 12, true},        // 2154-04-28 penumbral
    {134, 9, 21, 19, 29, 55, true},      // 2154-09-21 penumbral
    {134, 10, 21, 9, 26, 4, true},     // 2154-10-21 penumbral
    {135, 3, 19, 3, 12, 105, false},     // 2155-03-19 total
    {135, 9, 11, 7, 3, 100, false},      // 2155-09-11 total
    {136, 3, 7, 6, 54, 128, false},      // 2156-03-07 total
    {136, 8, 30, 23, 20, 141, false},    // 2156-08-30 total
    {137, 2, 24, 6, 16, 0, false},       // 2157-02-24 partial
    {137, 8, 20, 15, 46, 4, false},      // 2157-08-20 partial
    {138, 1, 14, 18, 29, 62, true},      // 2158-01-14 penumbral
    {138, 7, 11, 15, 55, 21, false},     // 2158-07-11 partial
    {139, 1, 4, 7, 19, 94, false},       // 2159-01-04 partial
    {139, 6, 30, 18, 0, 158, false},     // 2159-06-30 total
    {139, 12, 24, 23, 0, 157, false},    // 2159-12-24 total
    {140, 6, 18, 19, 10, 68, false},     // 2160-06-18 partial
    {140, 12, 13, 12, 50, 26, false},    // 2160-12-13 partial
    {141, 5, 9, 16, 38, 50, true},       // 2161-05-09 penumbral
    {141, 6, 8, 2, 8, 39, true},         // 2161-06-08 penumbral
    {141, 11, 3, 3, 45, 47, true},       // 2161-11-03 penumbral
    {142, 4, 29, 8, 17, 91, false},      // 2162-04-29 partial
    {142, 10, 23, 3, 23, 65, false},     // 2162-10-23 partial
    {143, 4, 19, 0, 49, 153, false},     // 2163-04-19 total
    {143, 10, 12, 5, 51, 165, false},    // 2163-10-12 total
    {144, 4, 7, 13, 34, 11, false},      // 2164-04-07 partial
    {144, 9, 30, 15, 40, 36, false},     // 2164-09-30 partial
    {145, 2, 26, 1, 44, 64, true},       // 2165-02-26 penumbral
    {145, 8, 21, 23, 34, 92, true},       // 2165-08-21 penumbral
    {145, 9, 20, 7, 5, 11, true},        // 2165-09-20 penumbral
    {146, 2, 15, 1, 11, 82, false},      // 2166-02-15 partial
    {146, 8, 11, 15, 17, 127, false},    // 2166-08-11 total
    {147, 2, 4, 5, 41, 151, false},      // 2167-02-04 total
    {147, 8, 1, 1, 28, 101, false},      // 2167-08-01 total
    {148, 1, 24, 17, 17, 31, false},     // 2168-01-24 partial
    {148, 7, 20, 4, 38, 61, true},       // 2168-07-20 penumbral
    {148, 12, 14, 21, 59, 66, true},     // 2168-12-14 penumbral
    {149, 1, 13, 8, 43, 13, true},       // 2169-01-13 penumbral
    {149, 6, 9, 15, 57, 34, false},      // 2169-06-09 partial
    {149, 12, 4, 10, 31, 84, false},     // 2169-12-04 partial
    {150, 5, 30, 0, 55, 183, false},     // 2170-05-30 total
    {150, 11, 23, 16, 16, 153, false},   // 2170-11-23 total
};
#define NUM_LUNAR_ECLIPSES (sizeof(lunar_eclipses) / sizeof(lunar_eclipse_t))

// Returns the table index of the eclipse on this local calendar date, or -1 if there isn't
// one. Table entries are UTC (see lunar_eclipse_t), so each is converted to local time (the
// same way _update_calendar does) before comparing -- an eclipse's UTC and local calendar
// days can differ, e.g. for an eclipse near 00:00 UTC, or a timezone far from UTC. When a
// match is found and out_local isn't NULL, also writes out that entry's local time (hour/
// minute included), so the caller can check visibility without redoing this conversion.
static int _find_eclipse(watch_date_time_t local_date_time, watch_date_time_t *out_local) {
    int32_t offset = movement_get_current_timezone_offset();
    for (size_t i = 0; i < NUM_LUNAR_ECLIPSES; i++) {
        // watch_date_time_t's year field is only 6 bits (0-63, i.e. 2020-2083 -- see
        // rtc_date_time_t in rtc32.h), but this table runs to year 150 (2170). An entry past
        // 63 would silently truncate into that range below (e.g. year 70 becomes 70 & 0x3F =
        // 6, aliasing 2090 as 2026) and could spuriously match some unrelated real date. The
        // device's own clock can't represent a year past 2083 in the first place, so such an
        // entry can never actually be "today" here -- skip it rather than risk that collision.
        if (lunar_eclipses[i].year > 63) continue;
        watch_date_time_t utc = {0};
        utc.unit.year = lunar_eclipses[i].year;
        utc.unit.month = lunar_eclipses[i].month;
        utc.unit.day = lunar_eclipses[i].day;
        utc.unit.hour = lunar_eclipses[i].hour;
        utc.unit.minute = lunar_eclipses[i].minute;
        watch_date_time_t local = watch_utility_date_time_from_unix_time(
            watch_utility_date_time_to_unix_time(utc, 0), offset);
        if (local.unit.year == local_date_time.unit.year &&
            local.unit.month == local_date_time.unit.month &&
            local.unit.day == local_date_time.unit.day) {
            if (out_local) *out_local = local;
            return (int) i;
        }
    }
    return -1;
}

// Returns the table index of the first eclipse on or after this date (comparing only
// year/month/day), or the last entry in the table if every eclipse in it is already past.
static size_t _find_eclipse_on_or_after(watch_date_time_t date_time) {
    for (size_t i = 0; i < NUM_LUNAR_ECLIPSES; i++) {
        const lunar_eclipse_t *e = &lunar_eclipses[i];
        if (e->year > date_time.unit.year) return i;
        if (e->year < date_time.unit.year) continue;
        if (e->month > date_time.unit.month) return i;
        if (e->month < date_time.unit.month) continue;
        if (e->day >= date_time.unit.day) return i;
    }
    return NUM_LUNAR_ECLIPSES - 1;
}

// Returns whether an eclipse at `local` (already converted to local time, e.g. by
// _find_eclipse's out_local) falls at night at the saved location -- i.e. is actually visible
// there. Eclipses only happen at full moon, and a full moon rises near sunset and sets near
// sunrise, so "is it night" is essentially "is the moon up", without needing a separate
// moon-position calculation. Returns false if no location is set, or if the sun doesn't
// rise/set that day at all (polar day/night -- left as "not visible" rather than guessing).
static bool _eclipse_visible_at_night(watch_date_time_t local, bool location_set) {
    if (!location_set) return false;
    movement_location_t location = {0};
    filesystem_read_file("location.u32", (char *) &location.reg, sizeof(movement_location_t));
    double lat = (double)(int16_t) location.bit.latitude / 100.0;
    double lon = (double)(int16_t) location.bit.longitude / 100.0;
    double rise, set;
    uint8_t result = sun_rise_set(WATCH_RTC_REFERENCE_YEAR + local.unit.year, local.unit.month, local.unit.day, lon, lat, &rise, &set);
    if (result != 0) return false;
    double hours_from_utc = (double) movement_get_timezone_offset_for_date(local) / 3600.0;
    double eclipse_hour = local.unit.hour + local.unit.minute / 60.0;
    return (eclipse_hour < rise + hours_from_utc) || (eclipse_hour > set + hours_from_utc);
}

void moon_phase_ascii_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(moon_phase_ascii_state_t));
        memset(*context_ptr, 0, sizeof(moon_phase_ascii_state_t));
    }
}

void moon_phase_ascii_face_activate(void *context) {
    (void) context;
}

// Segment bit indices, matching fluid_face.c's convention (bit 0 = segment A .. bit 7 = H).
#define SEG_A (1 << 0)
#define SEG_B (1 << 1)
#define SEG_C (1 << 2)
#define SEG_D (1 << 3)
#define SEG_E (1 << 4)
#define SEG_F (1 << 5)

// Lights exactly the given segments (and clears the rest) at a raw display position. Bypasses
// the font -- needed for glyphs no character in Custom_LCD_Character_Set matches exactly (e.g.
// the plain double-bar "=" look, which is just segments A+D; its actual '=' character is the
// middle+bottom bars instead) -- via the same digit mapping watch_display_character() itself
// uses.
static void _display_segments(uint8_t position, uint8_t mask) {
    if (watch_get_lcd_type() != WATCH_LCD_TYPE_CUSTOM) return;

    digit_mapping_t segmap = Custom_LCD_Display_Mapping[position];
    for (int i = 0; i < 8; i++) {
        if (segmap.segment[i].value == segment_does_not_exist) continue;
        if (mask & (1 << i)) watch_set_pixel(segmap.segment[i].address.com, segmap.segment[i].address.seg);
        else watch_clear_pixel(segmap.segment[i].address.com, segmap.segment[i].address.seg);
    }
}

// Writes into out[4] a plain moon-phase-style shape (same glyphs as ascii_art_moon's waxing
// entries) with `lit` of its 4 cells lit from the left -- e.g. lit=2 gives "[=  ", a plain
// half moon. Used to represent the fraction of the moon NOT covered during an eclipse: `lit`
// is magnitude_pct's complement scaled to 4 cells and rounded, so 50% magnitude gives lit=2,
// 100%+ gives lit=0 (blank -- fully covered). Not mirrored for hemisphere: unlike moon phase
// itself, an eclipse's shadow isn't a left/right-oriented sliver, so there's no "correct" side
// for this to grow from; picking one consistently is enough.
static void _eclipse_coverage_cells(uint8_t magnitude_pct, char out[4]) {
    static const char *const by_count[5] = { "    ", "[   ", "[=  ", "[== ", "[==]" };
    int lit = (4 * (100 - (int) magnitude_pct) + 50) / 100;
    if (lit < 0) lit = 0;
    if (lit > 4) lit = 4;
    memcpy(out, by_count[lit], 4);
}

// Draws the 4-cell phase bar across WATCH_POSITION_HOURS + WATCH_POSITION_MINUTES (raw
// positions 4-7). If state->eclipse_bar_magnitude > 0, the whole bar blinks every second (as
// state->blink_on toggles) between the given `cells` (always "[==]" when this applies, since
// eclipses only happen at full moon) and _eclipse_coverage_cells' shape for that magnitude;
// otherwise `cells` is just shown steadily. Cheap enough to call every second from EVENT_TICK
// without redoing the rest of _update().
static void _draw_phase_bar(moon_phase_ascii_state_t *state, const char cells[4]) {
    char coverage_cells[4];
    if (state->eclipse_bar_magnitude > 0 && !state->blink_on) {
        _eclipse_coverage_cells(state->eclipse_bar_magnitude, coverage_cells);
        cells = coverage_cells;
    }
    for (int i = 0; i < 4; i++) {
        uint8_t position = 4 + i;
        if (cells[i] == '=') {
            _display_segments(position, SEG_A | SEG_D);
        } else if (cells[i] == '|') {
            // '|' only ever lands in the outermost cell (0 or 3, see ascii_art_moon) --
            // light that cell's near vertical edge: left (E+F) at cell 0, right (B+C) at
            // cell 3. The font's own '|' is overridden elsewhere as the "ll" ligature and
            // would light both verticals regardless of position, so this needs raw segments.
            _display_segments(position, i == 0 ? (SEG_E | SEG_F) : (SEG_B | SEG_C));
        } else {
            watch_display_character(cells[i], position);
        }
    }
}

// Meeus, "Astronomical Algorithms" 2nd ed., ch. 49 ("Phases of the Moon"): the Julian
// Ephemeris Day of the new moon for lunation k (k=0 is 2000-01-06 ~18:15 UTC). The mean term
// alone (first line) is exactly the old FIRST_MOON-epoch/constant-LUNAR_DAYS calculation this
// replaces -- accurate only to the extent the moon's orbital speed actually were constant,
// which it isn't (real synodic months range ~29.18-29.93 days as Earth-Moon distance varies
// through the elliptical orbit), so that calculation could be off by over half a day at
// essentially any point in time (verified against a proper ephemeris) purely from this
// non-uniformity, not from any error accumulating with time since the epoch. The periodic
// correction terms below (in the Sun's mean anomaly M, the Moon's mean anomaly M', the
// Moon's argument of latitude F, and the longitude of its ascending node Omega) account for
// that non-uniformity and bring this to within ~2 minutes of a full ephemeris across
// 2000-2040 (checked against PyEphem) -- Meeus's own further "planetary argument" terms
// (his A1..A14), worth at most a few seconds each, are omitted as unnecessary for a display
// that only ever shows tenths of a day.
static double _moon_new_moon_jde(double k) {
    double T = k / 1236.85;
    double T2 = T * T, T3 = T2 * T, T4 = T3 * T;
    double jde = 2451550.09766 + LUNAR_DAYS * k + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;

    double E = 1 - 0.002516 * T - 0.0000074 * T2;
    double M  = fmod(2.5534   + 29.10535670  * k - 0.0000014 * T2 - 0.00000011 * T3, 360.0) * MOON_DEGRAD;
    double Mp = fmod(201.5643 + 385.81693528 * k + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4, 360.0) * MOON_DEGRAD;
    double F  = fmod(160.7108 + 390.67050284 * k - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4, 360.0) * MOON_DEGRAD;
    double Om = fmod(124.7746 - 1.56375588   * k + 0.0020672 * T2 + 0.00000215 * T3, 360.0) * MOON_DEGRAD;

    double correction =
        -0.40720 * sin(Mp)
        + 0.17241 * E * sin(M)
        + 0.01608 * sin(2 * Mp)
        + 0.01039 * sin(2 * F)
        + 0.00739 * E * sin(Mp - M)
        - 0.00514 * E * sin(Mp + M)
        + 0.00208 * E * E * sin(2 * M)
        - 0.00111 * sin(Mp - 2 * F)
        - 0.00057 * sin(Mp + 2 * F)
        + 0.00056 * E * sin(2 * Mp + M)
        - 0.00042 * sin(3 * Mp)
        + 0.00042 * E * sin(M + 2 * F)
        + 0.00038 * E * sin(M - 2 * F)
        - 0.00024 * E * sin(2 * Mp - M)
        - 0.00017 * sin(Om)
        - 0.00007 * sin(Mp + 2 * M)
        + 0.00004 * sin(2 * Mp - 2 * F)
        + 0.00004 * sin(3 * M)
        + 0.00003 * sin(Mp + M - 2 * F)
        + 0.00003 * sin(2 * Mp + 2 * F)
        - 0.00003 * sin(Mp + M + 2 * F)
        + 0.00003 * sin(Mp - M + 2 * F)
        - 0.00002 * sin(Mp - M - 2 * F)
        - 0.00002 * sin(3 * Mp + M)
        + 0.00002 * sin(4 * Mp);

    return jde + correction;
}

// Age of the moon (days since the preceding new moon) at the given unix time, via the
// above. k starts as a floor()'d estimate from the mean rate, which the periodic correction
// can occasionally shift across a lunation boundary -- the two checks below correct that by
// re-testing the adjacent lunation, so the result is always relative to the true preceding
// new moon rather than an off-by-one-lunation neighbor.
static double _moon_age_days(uint32_t now_unix) {
    double jd = (double) now_unix / 86400.0 + 2440587.5;
    double k = floor((jd - 2451550.09766) / LUNAR_DAYS);
    double jde = _moon_new_moon_jde(k);
    if (jde > jd) {
        k -= 1.0;
        jde = _moon_new_moon_jde(k);
    } else {
        double jde_next = _moon_new_moon_jde(k + 1.0);
        if (jde_next <= jd) jde = jde_next;
    }
    return jd - jde;
}

static void _update(moon_phase_ascii_state_t *state) {
    char buf[6];
    bool southern = state->southern_hemisphere;
    // watch_rtc_get_date_time()/watch_rtc_get_unix_time() are already UTC (see
    // movement_get_utc_date_time(), which returns the former verbatim) -- converting through
    // watch_utility_date_time_to_unix_time() with a nonzero utc_offset would subtract the
    // timezone offset a second time (it already isn't present to begin with), silently
    // shifting `now` away from true UTC by that same amount every time this runs.
    uint32_t now = watch_rtc_get_unix_time() + state->offset;
    watch_date_time_t date_time = watch_utility_date_time_from_unix_time(now, movement_get_current_timezone_offset());
    double currentday = _moon_age_days(now);
    uint8_t phase_index = 0;

    for(phase_index = 0; phase_index <= NUM_PHASES; phase_index++) {
        if (currentday > phase_changes[phase_index] && currentday <= phase_changes[phase_index + 1]) break;
    }

    // Top row: moon age in days. Integer part is variable width (no leading
    // zero); fractional part is always exactly 1 digit. Right-aligned with a
    // fixed trailing blank, so the tenths digit always lands in the same
    // spot regardless of whether the integer part is 1 or 2 digits.
    int tenths = (int)(currentday * 10.0 + 0.5); // 0-295 in practice (age is 0-29.5 days),
                                                  // so content below is always 2-3 characters --
                                                  // gcc can't see that bound through the double
                                                  // math above, hence the pragmas.
    char content[16];
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(content, sizeof(content), "%d%d", tenths / 10, tenths % 10);
    snprintf(buf, sizeof(buf), "%*s ", 4, content);
    #pragma GCC diagnostic pop
    // watch_display_text() only writes the first 2 of TOP's 5 characters (it
    // falls through to the same case as WATCH_POSITION_TOP_LEFT); the other 3
    // -- including the two that alias WATCH_POSITION_TOP_RIGHT -- only get
    // written via this fallback variant's TOP-specific position mapping.
    watch_display_text_with_fallback(WATCH_POSITION_TOP, buf, buf);

    // Hours/minutes: the 4-cell phase bar.
    const char *art = ascii_art_moon[phase_index];
    char cells[4];
    if (southern) {
        memcpy(cells, art, 4);
    } else {
        // mirror: reverse the cell order AND swap '[' <-> ']' so the leading/
        // trailing edge glyphs still point the right way round.
        for (int i = 0; i < 4; i++) {
            char c = art[3 - i];
            if (c == '[') c = ']';
            else if (c == ']') c = '[';
            cells[i] = c;
        }
    }
    // Eclipses only happen at full moon, so this only ever matters when phase_index == 4 --
    // but it's harmless to always check (the table just won't match on any other day). Unlike
    // a solar eclipse, a lunar eclipse is a location-independent fact (the Moon passes through
    // Earth's shadow at the same moment for every observer), so this doesn't need location_set
    // -- only the calendar mode's "visible at night from here" indicator does.
    watch_date_time_t eclipse_local = {0};
    int eclipse_index = _find_eclipse(date_time, &eclipse_local);
    bool penumbral = eclipse_index >= 0 && lunar_eclipses[eclipse_index].penumbral;
    uint8_t magnitude_pct = eclipse_index >= 0 ? lunar_eclipses[eclipse_index].magnitude_pct : 0;
    // A penumbral eclipse has no umbral magnitude to size a bar shape by, and a magnitude of
    // exactly 0% (e.g. 2157-02-24's recorded partial eclipse) wouldn't produce a visibly
    // different alternate shape anyway -- both instead blink just the colon, leaving the bar
    // showing the ordinary (always-full, since eclipses only happen at full moon) moon steadily.
    bool colon_only = eclipse_index >= 0 && (penumbral || magnitude_pct == 0);
    state->eclipse_bar_magnitude = (eclipse_index >= 0 && !colon_only) ? magnitude_pct : 0;
    state->blink_on = true; // always start a fresh eclipse day (or a fresh non-eclipse day) fully lit

    // The bar blink is driven by EVENT_TICK, one toggle per tick -- bump to 2 ticks/second so
    // each shape shows for 0.5s (a 1s full cycle) instead of the default 1Hz's 1s/2s. Only
    // while actually blinking something, since a faster tick costs more power; _update()
    // re-evaluates this every hour (or on offset/activate), so it drops back to 1 on its own
    // once the eclipse day has passed.
    movement_request_tick_frequency(state->eclipse_bar_magnitude > 0 ? 2 : 1);

    _draw_phase_bar(state, cells);

    // Uses the hardware's own autonomous indicator blink (unlike the bar above, this needs no
    // per-tick help: see watch_start_indicator_blink_if_possible). 250ms (instead of the
    // default-feeling 500ms) to match the bar blink's speed above.
    if (colon_only) watch_start_indicator_blink_if_possible(WATCH_INDICATOR_COLON, 250);
    else watch_clear_colon();

    // The sleep indicator (the same crescent-moon icon shown in low energy mode) lights up
    // if today's eclipse falls at night at the saved location -- same as calendar mode's
    // indicator, so it stays lit/cleared correctly across mode transitions without extra
    // bookkeeping there (see the LOW_ENERGY_UPDATE and ALARM_LONG_PRESS handlers below).
    bool visible = eclipse_index >= 0 && _eclipse_visible_at_night(eclipse_local, state->location_set);
    if (visible) watch_set_indicator(WATCH_INDICATOR_SLEEP);
    else watch_clear_indicator(WATCH_INDICATOR_SLEEP);

    // Seconds: day of the month.
    sprintf(buf, "%2d", date_time.unit.day);
    watch_display_text(WATCH_POSITION_SECONDS, buf);
}

// Renders calendar mode: browsing lunar_eclipses[state->calendar_index] independent of
// state->offset. Top-left shows the eclipse rate (magnitude_pct; a couple of total eclipses
// run over 100%, using all 3 of TOP_LEFT's slots for those -- see the comment at that
// sprintf); top-right the last 2 digits of the year; the
// hours/minutes/seconds positions the month, day, and hour, all local to the wearer (the
// table stores UTC). The sleep indicator (the same crescent-moon icon shown in low energy
// mode) lights up if the eclipse's local moment falls at night at the saved location -- since
// eclipses only happen at full moon, and a full moon rises near sunset and sets near sunrise,
// "is it night" is essentially "is the moon up", without needing a separate moon-position
// calculation.
static void _update_calendar(moon_phase_ascii_state_t *state) {
    const lunar_eclipse_t *e = &lunar_eclipses[state->calendar_index];
    char buf[6];

    // watch_date_time_t's year field is only 6 bits (2020-2083 -- see rtc_date_time_t in
    // rtc32.h), but this table runs to year 150 (2170); an entry past 63 can't be converted to
    // local time through it at all (same limitation as _find_eclipse, see its comment). Rather
    // than let that silently alias into some unrelated, wrong year, fall back to the entry's
    // raw UTC date/time for those -- up to a day off from the wearer's actual local calendar
    // day, but never a wrong year.
    bool locally_convertible = e->year <= 63;
    watch_date_time_t local = {0};
    if (locally_convertible) {
        watch_date_time_t utc = {0};
        utc.unit.year = e->year;
        utc.unit.month = e->month;
        utc.unit.day = e->day;
        utc.unit.hour = e->hour;
        utc.unit.minute = e->minute;
        local = watch_utility_date_time_from_unix_time(
            watch_utility_date_time_to_unix_time(utc, 0),
            movement_get_current_timezone_offset()
        );
    } else {
        local.unit.month = e->month;
        local.unit.day = e->day;
        local.unit.hour = e->hour;
        local.unit.minute = e->minute;
    }

    // TOP_LEFT has 3 character slots, but plain watch_display_text() only ever writes the
    // first 2 and never clears the 3rd -- use the _with_fallback variant (which does reach
    // it) and always supply a 3rd character so a stale digit from a previous screen can't
    // linger there. Above 100% (a couple of total eclipses run over), use all 3 slots to
    // show the real value; 100% and under fits in 2 digits, right-justified in the first
    // 2 slots (ones digit in the 2nd), with the 3rd slot blanked.
    if (e->magnitude_pct > 100) sprintf(buf, "%3d", e->magnitude_pct);
    else sprintf(buf, "%2d ", e->magnitude_pct);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, buf, buf);

    sprintf(buf, "%02d", (WATCH_RTC_REFERENCE_YEAR + (locally_convertible ? local.unit.year : e->year)) % 100);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);

    sprintf(buf, "%2d", local.unit.month);
    watch_display_text(WATCH_POSITION_HOURS, buf);
    sprintf(buf, "%2d", local.unit.day);
    watch_display_text(WATCH_POSITION_MINUTES, buf);
    sprintf(buf, "%02d", local.unit.hour); // 24-hour, always 2 digits
    watch_display_text(WATCH_POSITION_SECONDS, buf);

    watch_clear_colon();
    // Marks the month/day as a date (not a HH:MM time) with a "." in place of the colon.
    watch_set_decimal_if_available();

    // Visibility isn't meaningfully computable without a real local year either (sun_rise_set
    // would get the same wrong year local.unit.year would carry) -- treat those entries as
    // not visible rather than guess, same as the polar day/night case inside the helper.
    if (locally_convertible && _eclipse_visible_at_night(local, state->location_set)) watch_set_indicator(WATCH_INDICATOR_SLEEP);
    else watch_clear_indicator(WATCH_INDICATOR_SLEEP);
}

bool moon_phase_ascii_face_loop(movement_event_t event, void *context) {
    moon_phase_ascii_state_t *state = (moon_phase_ascii_state_t *)context;
    watch_date_time_t date_time;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            if (watch_sleep_animation_is_running()) watch_stop_sleep_animation();
            state->location_set = _read_location(state);
            if (state->calendar_mode) _update_calendar(state);
            else _update(state);
            break;
        case EVENT_TICK:
            // Calendar mode is a static browse, no per-second animation.
            if (state->calendar_mode) break;
            date_time = watch_rtc_get_date_time();
            if ((date_time.unit.minute == 0) && (date_time.unit.second == 0)) {
                // only update everything once an hour...
                _update(state);
            } else if (state->eclipse_bar_magnitude > 0) {
                // ...but keep blinking the eclipse bar every second in between. Eclipses only
                // happen at full moon, which is left-right symmetric, so the un-mirrored,
                // un-blinked "[==]" is correct for both hemispheres here.
                static const char full_moon_cells[4] = {'[', '=', '=', ']'};
                state->blink_on = !state->blink_on;
                _draw_phase_bar(state, full_moon_cells);
            }
            break;
        case EVENT_LOW_ENERGY_UPDATE: {
            // Entering low energy mode always drops out of calendar mode and back to today's
            // moon phase -- simpler than teaching calendar mode its own sleep-mode rendering,
            // and calendar browsing isn't something you'd want to leave running unattended.
            bool left_calendar_mode = state->calendar_mode;
            if (state->calendar_mode) {
                state->calendar_mode = false;
                watch_clear_decimal_if_available();
            }
            // update at the top of the hour, or if we're entering sleep mode with an offset,
            // or if we just left calendar mode -- that last one matters even with no other
            // reason to update, since _update() is what sets/clears the sleep indicator for
            // today's actual eclipse visibility; without it, the indicator would be left
            // however calendar mode last drew it instead. Also, in sleep mode, always show
            // the current moon phase (offset = 0).
            if (left_calendar_mode || state->offset || (watch_rtc_get_date_time().unit.minute == 0)) {
                state->offset = 0;
                _update(state);
            }
            // and kill the offset so when the wearer wakes up, it matches what's on screen.
            state->offset = 0;
            if (!watch_sleep_animation_is_running()) watch_start_sleep_animation(1000);
            break;
        }
        case EVENT_ALARM_BUTTON_UP:
            if (state->calendar_mode) {
                // next eclipse
                if (state->calendar_index < NUM_LUNAR_ECLIPSES - 1) state->calendar_index++;
                _update_calendar(state);
            } else {
                // Pressing the alarm adds an offset of one day to the displayed value,
                // so you can see moon phases in the future.
                state->offset += 86400;
                _update(state);
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->calendar_mode) {
                // back to today's moon phase -- _update() below sets/clears the sleep
                // indicator itself based on today's actual eclipse visibility, so no need
                // to clear it separately here first.
                state->calendar_mode = false;
                watch_clear_decimal_if_available();
                state->offset = 0;
                _update(state);
            } else {
                // into the eclipse calendar, starting from the next eclipse from today
                state->calendar_mode = true;
                state->calendar_index = _find_eclipse_on_or_after(watch_rtc_get_date_time());
                _update_calendar(state);
            }
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->calendar_mode) {
                // previous eclipse
                if (state->calendar_index > 0) state->calendar_index--;
                _update_calendar(state);
            } else {
                state->offset -= 86400;
                _update(state);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            movement_illuminate_led();
            break;
        case EVENT_TIMEOUT:
            // QUESTION: Should timeout reset offset to 0?
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void moon_phase_ascii_face_resign(void *context) {
    moon_phase_ascii_state_t *state = (moon_phase_ascii_state_t *)context;
    state->offset = 0;
    movement_request_tick_frequency(1); // in case an eclipse bar blink had bumped this to 2
    if (state->calendar_mode) {
        state->calendar_mode = false;
        watch_clear_indicator(WATCH_INDICATOR_SLEEP);
        watch_clear_decimal_if_available();
    }
}
