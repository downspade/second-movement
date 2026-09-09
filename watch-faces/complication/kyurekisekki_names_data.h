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

#ifndef KYUREKISEKKI_NAMES_DATA_H_
#define KYUREKISEKKI_NAMES_DATA_H_

// 二十四節気 (24 solar terms)
// 立春（RISSHUN) 雨水（USUI) 啓蟄（KEITITSU）
// 春分（SHUNBUN）清明（SEIMEI）穀雨（KOKUU）
// 立夏（RIKKA）小満（SYOMAN）芒種（BOUSHU）
// 夏至（GESHI）小暑（SYOUSHO）大暑（TAISHO）
// 立秋（RISSYU）処暑（SHOSHO）白露（HAKURO）
// 秋分（SYUBUN）寒露（KANRO）霜降（SOUKOU）
// 立冬（RITTOU）小雪（SYOSETSU）大雪（TAISETSU）
// 冬至（TOHJI）小寒（SYOKAN）大寒（DAIKAN）
static const char *sekki_names_5[24] = { // 5 chars, custom LCD TOP
    "RISHN", "USUl ", "KTITU",
    "SHNBN", "SEMEl", "KOKUU",
    "RIKKA", "SYMAN", "BOSHU",
    "GESHl", "SYOSH", "TAISH",
    "RISSY", "SHSHO", "HAKRO",
    "SYUBN", "KANRO", "SOKOU",
    "RITTO", "SYOST", "TAIST",
    "TOHJl", "SYOKN", "DAIKN",
};

static const char *sekki_names_6[24] = { // 6 chars, classic LCD BOTTOM (alternated with the date)
    "RISSHN", "USUI  ", "KETITU",
    "SHNBUN", "SEIMEI", "KOKUU ",
    "RIKKA ", "SYOMAN", "BOUSHU",
    "GESHI ", "SYOSHO", "TAISHO",
    "RISSYU", "SHSHOU", "HAKURO",
    "SYUBUN", "KANRO ", "SOUKOU",
    "RITTO ", "SYOSTU", "TAISTU",
    "TOHJI ", "SYOKAN", "DAIKAN",
};

#endif // KYUREKISEKKI_NAMES_DATA_H_
