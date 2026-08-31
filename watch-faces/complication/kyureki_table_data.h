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

#ifndef KYUREKI_TABLE_DATA_H_
#define KYUREKI_TABLE_DATA_H_

/*
 * Generated table: Japanese lunisolar calendar (kyureki), one entry per Gregorian
 * year from KYUREKI_TABLE_BASE_YEAR to KYUREKI_TABLE_BASE_YEAR + (N-1).
 *
 * Computed offline from JPL DE440 ephemeris positions (skyfield), using the
 * standard "true motion" algorithm: New Year = the new moon that starts the
 * first lunar month after the winter-solstice-containing month (month 11);
 * a leap month is inserted at the first month (after month 11) in a 13-month
 * lunar year that contains no zhongqi (中气). All day boundaries use JST (UTC+9).
 *
 * Accuracy note: predictions this far into the future inherently depend on
 * assumptions about Earth's future rotation (ΔT) that cannot be verified in
 * advance; this is a limitation of any multi-century calendar prediction, not
 * specific to this table.
 */

#define KYUREKI_TABLE_BASE_YEAR 2025
#define KYUREKI_TABLE_NUM_YEARS 151

typedef struct {
    uint8_t  days_to_new_year;  // day-of-year (0-indexed, JST) that the lunar new year falls on
    uint8_t  leap_month;        // 0 = no leap month; 1-12 = leap month follows this month number
    uint16_t month_lengths;     // bit i (i=0 is month 1) = 1 -> 30-day month, 0 -> 29-day month
} lunar_year_t;

static const lunar_year_t kyureki_table[KYUREKI_TABLE_NUM_YEARS] = {
    {  28,  6,  3749 }, // 2025: new year 2025-01-29
    {  47,  0,  3749 }, // 2026: new year 2026-02-17
    {  37,  0,  3658 }, // 2027: new year 2027-02-07
    {  26,  5,  3222 }, // 2028: new year 2028-01-27
    {  43,  0,  3227 }, // 2029: new year 2029-02-13
    {  33,  0,  1370 }, // 2030: new year 2030-02-03
    {  22,  3,  2773 }, // 2031: new year 2031-01-23
    {  41,  0,  2921 }, // 2032: new year 2032-02-11
    {  30,  7,  5970 }, // 2033: new year 2033-01-31
    {  49,  0,  1874 }, // 2034: new year 2034-02-19
    {  38,  0,  2853 }, // 2035: new year 2035-02-08
    {  27,  5,  5707 }, // 2036: new year 2036-01-28
    {  45,  0,  2635 }, // 2037: new year 2037-02-15
    {  34,  0,  1195 }, // 2038: new year 2038-02-04
    {  23,  4,  1371 }, // 2039: new year 2039-01-24
    {  42,  0,  1389 }, // 2040: new year 2040-02-12
    {  31,  0,  2921 }, // 2041: new year 2041-02-01
    {  21,  2,  6994 }, // 2042: new year 2042-01-22
    {  40,  0,  3474 }, // 2043: new year 2043-02-10
    {  29,  6,  7461 }, // 2044: new year 2044-01-30
    {  47,  0,  3365 }, // 2045: new year 2045-02-17
    {  36,  0,  2637 }, // 2046: new year 2046-02-06
    {  25,  5,  5293 }, // 2047: new year 2047-01-26
    {  44,  0,   694 }, // 2048: new year 2048-02-14
    {  32,  0,  1461 }, // 2049: new year 2049-02-02
    {  22,  3,  3497 }, // 2050: new year 2050-01-23
    {  41,  0,  3753 }, // 2051: new year 2051-02-11
    {  31,  7,  7570 }, // 2052: new year 2052-02-01
    {  49,  0,  3730 }, // 2053: new year 2053-02-19
    {  38,  0,  3366 }, // 2054: new year 2054-02-08
    {  27,  6,  2646 }, // 2055: new year 2055-01-28
    {  45,  0,  2647 }, // 2056: new year 2056-02-15
    {  34,  0,  1238 }, // 2057: new year 2057-02-04
    {  23,  4,  1717 }, // 2058: new year 2058-01-24
    {  42,  0,  1749 }, // 2059: new year 2059-02-12
    {  32,  0,  3785 }, // 2060: new year 2060-02-02
    {  21,  2,  3730 }, // 2061: new year 2061-01-22
    {  39,  0,  1683 }, // 2062: new year 2062-02-09
    {  28,  6,  5419 }, // 2063: new year 2063-01-29
    {  47,  0,  1323 }, // 2064: new year 2064-02-17
    {  35,  0,  2651 }, // 2065: new year 2065-02-05
    {  25,  5,  5466 }, // 2066: new year 2066-01-26
    {  44,  0,  1386 }, // 2067: new year 2067-02-14
    {  33,  0,  2901 }, // 2068: new year 2068-02-03
    {  22,  3,  5961 }, // 2069: new year 2069-01-23
    {  41,  0,  2889 }, // 2070: new year 2070-02-11
    {  30,  7,  6803 }, // 2071: new year 2071-01-31
    {  49,  0,  2709 }, // 2072: new year 2072-02-19
    {  37,  0,  1325 }, // 2073: new year 2073-02-07
    {  26,  6,  2669 }, // 2074: new year 2074-01-27
    {  45,  0,  2741 }, // 2075: new year 2075-02-15
    {  35,  0,  1450 }, // 2076: new year 2076-02-05
    {  23,  4,  2981 }, // 2077: new year 2077-01-24
    {  42,  0,  3493 }, // 2078: new year 2078-02-12
    {  32,  9,  7498 }, // 2079: new year 2079-02-02
    {  51,  0,  3402 }, // 2080: new year 2080-02-21
    {  39,  0,  3221 }, // 2081: new year 2081-02-09
    {  28,  6,  5422 }, // 2082: new year 2082-01-29
    {  47,  0,  1366 }, // 2083: new year 2083-02-17
    {  36,  0,  2741 }, // 2084: new year 2084-02-06
    {  25,  4,  5554 }, // 2085: new year 2085-01-26
    {  44,  0,  1746 }, // 2086: new year 2086-02-14
    {  33,  0,  3749 }, // 2087: new year 2087-02-03
    {  23,  3,  7754 }, // 2088: new year 2088-01-24
    {  41,  0,  1610 }, // 2089: new year 2089-02-11
    {  29,  7,  3223 }, // 2090: new year 2090-01-30
    {  48,  0,  3243 }, // 2091: new year 2091-02-18
    {  38,  0,  1370 }, // 2092: new year 2092-02-08
    {  26,  5,  2773 }, // 2093: new year 2093-01-27
    {  45,  0,  2921 }, // 2094: new year 2094-02-15
    {  35,  0,  1874 }, // 2095: new year 2095-02-05
    {  24,  4,  3749 }, // 2096: new year 2096-01-25
    {  42,  0,  2853 }, // 2097: new year 2097-02-12
    {  31,  8,  5707 }, // 2098: new year 2098-02-01
    {  50,  0,  2635 }, // 2099: new year 2099-02-20
    {  39,  0,  1195 }, // 2100: new year 2100-02-09
    {  28,  6,  1371 }, // 2101: new year 2101-01-29
    {  47,  0,  1453 }, // 2102: new year 2102-02-17
    {  37,  0,  2921 }, // 2103: new year 2103-02-07
    {  27,  4,  6994 }, // 2104: new year 2104-01-28
    {  45,  0,  3474 }, // 2105: new year 2105-02-15
    {  34,  0,  3365 }, // 2106: new year 2106-02-04
    {  23,  3,  6731 }, // 2107: new year 2107-01-24
    {  42,  0,  2645 }, // 2108: new year 2108-02-12
    {  30,  7,  5293 }, // 2109: new year 2109-01-31
    {  49,  0,  1206 }, // 2110: new year 2110-02-19
    {  38,  0,  1461 }, // 2111: new year 2111-02-08
    {  28,  5,  3498 }, // 2112: new year 2112-01-29
    {  46,  0,  3785 }, // 2113: new year 2113-02-16
    {  36,  0,  3730 }, // 2114: new year 2114-02-06
    {  25,  4,  7461 }, // 2115: new year 2115-01-26
    {  44,  0,  3366 }, // 2116: new year 2116-02-14
    {  32,  8,  2646 }, // 2117: new year 2117-02-02
    {  50,  0,  2647 }, // 2118: new year 2118-02-20
    {  40,  0,  1238 }, // 2119: new year 2119-02-10
    {  29,  6,  2773 }, // 2120: new year 2120-01-30
    {  47,  0,  1749 }, // 2121: new year 2121-02-17
    {  37,  0,  3785 }, // 2122: new year 2122-02-07
    {  27,  5,  3730 }, // 2123: new year 2123-01-28
    {  45,  0,  1683 }, // 2124: new year 2124-02-15
    {  33,  0,  1323 }, // 2125: new year 2125-02-03
    {  22,  3,  2647 }, // 2126: new year 2126-01-23
    {  41,  0,  2651 }, // 2127: new year 2127-02-11
    {  31,  7,  5466 }, // 2128: new year 2128-02-01
    {  49,  0,  1386 }, // 2129: new year 2129-02-19
    {  38,  0,  2917 }, // 2130: new year 2130-02-08
    {  28,  5,  5962 }, // 2131: new year 2131-01-29
    {  47,  0,  2889 }, // 2132: new year 2132-02-17
    {  35,  0,  2709 }, // 2133: new year 2133-02-05
    {  24,  4,  5419 }, // 2134: new year 2134-01-25
    {  43,  0,  1325 }, // 2135: new year 2135-02-13
    {  32,  8,  2733 }, // 2136: new year 2136-02-02
    {  50,  0,  2741 }, // 2137: new year 2137-02-20
    {  40,  0,  1450 }, // 2138: new year 2138-02-10
    {  29,  6,  2981 }, // 2139: new year 2139-01-30
    {  48,  0,  3493 }, // 2140: new year 2140-02-18
    {  37,  0,  3402 }, // 2141: new year 2141-02-07
    {  26,  5,  7317 }, // 2142: new year 2142-01-27
    {  45,  0,  3222 }, // 2143: new year 2143-02-15
    {  34,  0,  2382 }, // 2144: new year 2144-02-04
    {  22,  2,  2733 }, // 2145: new year 2145-01-23
    {  41,  0,  2741 }, // 2146: new year 2146-02-11
    {  31,  7,  5554 }, // 2147: new year 2147-02-01
    {  50,  0,  1746 }, // 2148: new year 2148-02-20
    {  38,  0,  3749 }, // 2149: new year 2149-02-08
    {  28,  5,  7754 }, // 2150: new year 2150-01-29
    {  47,  0,  1674 }, // 2151: new year 2151-02-17
    {  35,  0,  3223 }, // 2152: new year 2152-02-05
    {  24,  3,  2390 }, // 2153: new year 2153-01-25
    {  42,  0,  1371 }, // 2154: new year 2154-02-12
    {  32,  8,  2774 }, // 2155: new year 2155-02-02
    {  51,  0,  2922 }, // 2156: new year 2156-02-21
    {  40,  0,  1874 }, // 2157: new year 2157-02-10
    {  29,  6,  5925 }, // 2158: new year 2158-01-30
    {  48,  0,  2853 }, // 2159: new year 2159-02-18
    {  37,  0,  1675 }, // 2160: new year 2160-02-07
    {  25,  5,  5275 }, // 2161: new year 2161-01-26
    {  44,  0,  1195 }, // 2162: new year 2162-02-14
    {  33,  0,  2395 }, // 2163: new year 2163-02-03
    {  23,  2,  2906 }, // 2164: new year 2164-01-24
    {  41,  0,  2986 }, // 2165: new year 2165-02-11
    {  31,  7,  6994 }, // 2166: new year 2166-02-01
    {  50,  0,  3474 }, // 2167: new year 2167-02-20
    {  39,  0,  3397 }, // 2168: new year 2168-02-09
    {  27,  5,  6731 }, // 2169: new year 2169-01-28
    {  46,  0,  2645 }, // 2170: new year 2170-02-16
    {  35,  0,  1197 }, // 2171: new year 2171-02-05
    {  24,  3,  2413 }, // 2172: new year 2172-01-25
    {  42,  0,  1461 }, // 2173: new year 2173-02-12
    {  32,  8,  3498 }, // 2174: new year 2174-02-02
    {  51,  0,  3785 }, // 2175: new year 2175-02-21
};

#endif // KYUREKI_TABLE_DATA_H_
