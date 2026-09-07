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

// Auto-generated from watch-library/simulator/shell.html classic-LCD segment artwork
// and watch-library/shared/watch/watch_common_display.h's Classic_LCD_Display_Mapping.
// Do not hand-edit; regenerate if either source changes.
//
// Symbol names deliberately match fluid_face_data.h's (custom LCD) 1:1 -- fluid_face.c
// includes exactly one of the two headers (see FORCE_CLASSIC_LCD_TYPE/FORCE_CUSTOM_LCD_TYPE)
// so there's no clash, and the rest of fluid_face.c doesn't need to know which display
// it's on to walk the pixel/adjacency tables.
#define FLUID_NUM_PIXELS 72

// Standard 7-segment digit font (A=bit0..G=bit6), identical convention/values to
// fluid_face_data.h's (custom LCD) copy and to Classic_LCD_Character_Set's own '0'..'9'
// entries -- duplicated here rather than shared since the two data headers are never
// included together.
static const uint8_t fluid_digit_font[10] = { 63, 6, 91, 79, 102, 109, 125, 7, 127, 111 };

static const uint8_t fluid_pixel_com[72] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };

static const uint8_t fluid_pixel_seg[72] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };

// Direction index order: 0=E, 1=SE, 2=S, 3=SW, 4=W, 5=NW, 6=N, 7=NE (45-degree steps, 2=S=straight down)
static const int8_t fluid_pixel_dir[8][72] = {
  { 2, 2, 4, 6, 5, 30, -1, -1, 31, 8, 57, 10, 11, 12, 37, 63, 14, 16, 19, 20, 45, 46, 24, 24, 1, 26, 51, 29, 53, -1, -1, -1, -1, 56, 10, 34, 60, 60, 36, 36, 22, 69, 20, 41, 40, 46, 23, 48, 58, 50, 52, 28, -1, -1, -1, -1, 55, 56, 50, 34, 35, 59, 61, 59, 15, 64, 67, 41, 70, 70, 71, 48 },
  { -1, 3, 3, -1, 6, 6, -1, 31, 32, 56, 52, 35, 60, 37, 39, 70, 15, 64, -1, 21, 21, -1, 23, 0, 0, 1, 27, 4, 29, 30, -1, -1, 55, 57, 50, 49, 59, 36, 61, 38, 46, 44, 19, 19, 45, -1, -1, 23, 25, 58, 51, 29, 53, -1, -1, -1, 54, 52, 26, 48, 49, 71, 63, 71, 68, 41, 43, 20, 69, 40, 47, 24 },
  { -1, -1, -1, -1, -1, -1, -1, 32, 56, 57, 50, 60, 36, 39, 62, 69, 64, 65, -1, -1, -1, -1, -1, -1, -1, 0, 2, 3, 5, 6, -1, 55, 54, 50, 49, 71, 70, 61, 63, 63, -1, 20, -1, -1, 21, -1, -1, -1, 24, 25, 27, 4, 29, 30, 52, 52, 52, 50, 1, 71, 59, 70, 69, 70, 41, 67, 18, 19, 44, 45, 46, 23 },
  { -1, 0, -1, -1, 3, 3, -1, 8, 57, 33, 49, 36, 37, 14, 64, 68, 65, -1, -1, -1, -1, -1, 46, -1, -1, 24, 1, 2, 27, 5, 6, 32, 56, 10, 48, 59, 61, 38, 62, 62, 45, 19, 18, 42, 20, 21, -1, 22, 23, 48, 26, 27, 28, 29, 50, 54, 50, 58, 25, 70, 63, 63, 15, 68, 67, -1, -1, 43, 41, 44, 40, 47 },
  { 46, 24, 1, 0, 2, 4, 3, 9, 9, 34, 34, 12, 13, 16, 16, 64, 17, -1, -1, 18, 19, 18, 40, 46, 23, 47, 25, 1, 51, 27, 5, 8, 9, 34, 35, 60, 38, 14, 16, 16, 44, 67, -1, -1, 43, 20, 45, 40, 71, 71, 58, 26, 51, 28, 57, 56, 57, 10, 48, 61, 36, 62, 64, 15, 65, -1, -1, 66, 67, 41, 69, 70 },
  { 24, 25, 25, 2, 27, 51, 5, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, 42, 67, 20, 70, 22, 71, 48, 58, 26, 50, 28, 29, 7, 8, -1, 11, 11, 37, 13, 39, 14, 69, 65, 66, 66, 41, 44, 40, 70, 59, 59, 34, 50, 57, 52, 56, 32, 9, 33, 49, 36, 12, 38, -1, 62, 17, -1, -1, -1, 64, 68, 15, 61 },
  { 25, 58, 26, 27, 51, 28, 29, -1, -1, -1, -1, -1, -1, -1, -1, 14, -1, -1, 66, 67, 41, 44, 59, 71, 48, 49, 10, 50, 54, 52, 53, -1, 7, -1, -1, -1, 12, -1, 13, 13, 63, 64, 65, 65, 68, 69, 70, 59, -1, 34, 10, 57, 54, 54, 32, 31, 8, 9, 34, 60, 11, 37, 14, 38, 16, 17, 65, 65, 16, 15, 63, 59 },
  { 1, 26, 27, 4, 29, 29, 30, -1, 7, -1, 33, -1, -1, -1, 13, 62, -1, -1, 42, 44, 44, 45, 47, 25, 25, 58, 50, 51, 52, 53, -1, -1, 31, 9, -1, -1, 11, 12, 37, 37, 70, 68, 43, 67, 69, 40, 22, 71, 49, 10, 54, 54, -1, -1, 55, -1, 32, 8, 10, 35, -1, 36, 39, 61, 14, 16, 64, 64, 15, 63, 59, 34 },
};

static const uint8_t fluid_dir_order[8][72] = {
  { 53, 30, 31, 55, 7, 52, 32, 29, 54, 6, 8, 56, 28, 5, 9, 57, 51, 4, 33, 50, 27, 3, 10, 26, 2, 58, 1, 34, 49, 25, 0, 48, 24, 71, 35, 23, 11, 60, 59, 47, 22, 36, 12, 70, 46, 37, 61, 13, 38, 39, 63, 40, 14, 62, 15, 69, 45, 68, 44, 21, 16, 64, 41, 20, 67, 19, 17, 65, 43, 42, 66, 18 },
  { 30, 6, 5, 29, 53, 4, 3, 28, 52, 2, 27, 51, 26, 55, 50, 54, 1, 0, 31, 32, 56, 24, 57, 7, 8, 25, 58, 9, 33, 10, 23, 48, 49, 46, 34, 22, 47, 71, 70, 40, 45, 21, 59, 35, 60, 20, 44, 69, 36, 11, 61, 63, 19, 12, 41, 37, 68, 15, 38, 39, 62, 13, 14, 18, 42, 43, 67, 64, 66, 16, 65, 17 },
  { 3, 6, 0, 21, 30, 4, 2, 5, 1, 19, 23, 45, 18, 20, 24, 46, 27, 29, 40, 22, 42, 25, 43, 44, 47, 26, 28, 53, 51, 50, 52, 41, 48, 66, 70, 58, 67, 69, 71, 49, 68, 54, 64, 59, 15, 63, 65, 55, 57, 56, 34, 10, 62, 61, 32, 33, 36, 35, 38, 39, 60, 8, 31, 9, 16, 17, 37, 14, 7, 11, 12, 13 },
  { 18, 19, 42, 43, 66, 21, 20, 67, 45, 44, 41, 65, 17, 69, 40, 68, 46, 64, 23, 22, 16, 47, 70, 15, 0, 24, 62, 63, 71, 14, 39, 38, 61, 1, 25, 48, 13, 59, 3, 37, 2, 36, 60, 12, 58, 49, 4, 35, 27, 26, 11, 6, 5, 51, 50, 34, 30, 29, 28, 53, 52, 10, 57, 33, 54, 56, 9, 55, 32, 8, 7, 31 },
  { 18, 66, 42, 43, 65, 17, 19, 67, 20, 41, 64, 16, 21, 44, 68, 45, 69, 15, 62, 14, 40, 63, 39, 38, 13, 61, 37, 46, 70, 12, 36, 22, 47, 59, 60, 11, 23, 35, 71, 24, 48, 0, 25, 49, 34, 1, 58, 2, 26, 10, 3, 27, 50, 33, 4, 51, 57, 9, 5, 28, 56, 8, 6, 54, 29, 32, 52, 7, 55, 31, 30, 53 },
  { 17, 65, 16, 66, 64, 67, 43, 42, 18, 14, 13, 62, 39, 38, 15, 68, 37, 41, 12, 19, 63, 61, 11, 36, 69, 44, 20, 60, 35, 59, 21, 45, 40, 70, 71, 47, 22, 34, 46, 49, 48, 23, 10, 33, 9, 58, 25, 8, 7, 57, 24, 56, 32, 31, 0, 1, 54, 50, 55, 26, 51, 27, 2, 52, 28, 3, 4, 53, 29, 5, 6, 30 },
  { 13, 12, 11, 7, 14, 37, 17, 16, 9, 31, 8, 60, 39, 38, 35, 36, 33, 32, 61, 62, 10, 34, 56, 57, 55, 65, 63, 15, 59, 64, 54, 49, 68, 58, 67, 69, 71, 41, 48, 66, 70, 50, 52, 51, 53, 26, 28, 25, 43, 44, 47, 22, 42, 40, 27, 29, 18, 20, 24, 46, 1, 19, 23, 45, 5, 2, 4, 30, 0, 21, 3, 6 },
  { 31, 7, 8, 32, 55, 9, 56, 54, 33, 57, 10, 52, 53, 28, 29, 30, 34, 50, 51, 5, 6, 11, 26, 27, 35, 4, 49, 58, 12, 60, 36, 2, 37, 3, 59, 13, 48, 25, 1, 61, 38, 39, 14, 71, 63, 62, 24, 0, 15, 70, 47, 16, 22, 23, 64, 46, 68, 40, 69, 17, 65, 41, 44, 45, 67, 20, 21, 66, 43, 42, 19, 18 },
};

static const int8_t fluid_hours_tens_pixel[8] = { 42, 67, 19, 42, 18, 66, 43, -1 };
static const int8_t fluid_hours_ones_pixel[8] = { 68, 69, 45, 21, 20, 41, 44, -1 };
static const int8_t fluid_minutes_tens_pixel[8] = { 22, 71, 23, 22, 46, 70, 47, -1 };
static const int8_t fluid_minutes_ones_pixel[8] = { 49, 58, 1, 0, 24, 48, 25, -1 };
static const int8_t fluid_seconds_tens_pixel[8] = { 50, 51, 4, 3, 2, 26, 27, -1 };
static const int8_t fluid_seconds_ones_pixel[8] = { 52, 53, 30, 6, 5, 28, 29, -1 };
static const int8_t fluid_day_tens_pixel[8] = { 33, 9, 57, 33, 10, -1, 33, -1 };
static const int8_t fluid_day_ones_pixel[8] = { 7, 31, 55, 54, 56, 8, 32, -1 };

// Weekday characters: classic only has 2 (custom has 3), Classic_LCD_Character_Set bit order.
static const int8_t fluid_weekday1_pixel[8] = { 13, 37, 61, 63, 62, 14, 39, 38 };
static const int8_t fluid_weekday2_pixel[8] = { 11, 35, 35, 59, 36, 36, 60, 12 };

#define FLUID_COLON_PIXEL 40
#define FLUID_SIGNAL_PIXEL 16   // alarm bell icon -> alarm_enabled
#define FLUID_BELL_PIXEL 17    // 5-bar "signal" icon -> time_signal_enabled (hourly chime)
#define FLUID_PM_PIXEL 65
#define FLUID_24H_PIXEL 64
// No low-battery (ARROWS) icon on the classic module -- FLUID_ARROWS_PIXEL intentionally omitted.
