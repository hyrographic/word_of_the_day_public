#include "varfonts.h"

#ifndef VARFONT_DEFAULT_SPACE_ADVANCE
#define VARFONT_DEFAULT_SPACE_ADVANCE 3
#endif

// Binary search for a codepoint in font->codepoints[]. Returns index or -1.
static int16_t VarFont_FindGlyphIndex(const sVARFONT* font, uint16_t codepoint)
{
    if (!font || font->glyph_count == 0) return -1;

    int32_t lo = 0;
    int32_t hi = (int32_t)font->glyph_count - 1;

    while (lo <= hi) {
        int32_t mid = (lo + hi) >> 1;
        uint16_t v = pgm_read_word(&font->codepoints[mid]);
        if (v == codepoint) return (int16_t)mid;
        if (v < codepoint) lo = mid + 1;
        else               hi = mid - 1;
    }
    return -1;
}

uint8_t VarFont_GetCharWidth(const sVARFONT* font, char c)
{
    const uint16_t cp = (uint8_t)c;
    const int16_t idx = VarFont_FindGlyphIndex(font, cp);
    if (idx < 0) return 0;
    return pgm_read_byte(&font->widths[idx]);
}

uint8_t VarFont_GetCharAdvance(const sVARFONT* font, char c)
{
    const uint16_t cp = (uint8_t)c;

    // Always advance for ASCII space even if it is not present in the font.
    if (cp == 32) {
        const int16_t idx = VarFont_FindGlyphIndex(font, 32);
        if (idx >= 0) {
            return pgm_read_byte(&font->advances[idx]);
        }
        return VARFONT_DEFAULT_SPACE_ADVANCE;
    }

    const int16_t idx = VarFont_FindGlyphIndex(font, cp);
    if (idx < 0) return 0;

    const uint8_t adv = pgm_read_byte(&font->advances[idx]);
    // Backstop: if a glyph exists but has zero advance, fall back to width.
    if (adv != 0) return adv;

    return pgm_read_byte(&font->widths[idx]);
}

uint32_t VarFont_GetCharOffset(const sVARFONT* font, char c)
{
    const uint16_t cp = (uint8_t)c;
    const int16_t idx = VarFont_FindGlyphIndex(font, cp);
    if (idx < 0) return 0;

    return pgm_read_dword(&font->offsets[idx]);
}

uint8_t DrawVarChar(int x, int y, char c, const sVARFONT* font, uint16_t color, bool draw, Paint& paint)
{
    const uint16_t cp = (uint8_t)c;

    // Handle space even if not present.
    if (cp == 32) {
        const int16_t idx = VarFont_FindGlyphIndex(font, 32);
        if (idx < 0) return VARFONT_DEFAULT_SPACE_ADVANCE;
        // If it exists but has no ink, just advance.
        const uint8_t adv = pgm_read_byte(&font->advances[idx]);
        const uint8_t w   = pgm_read_byte(&font->widths[idx]);
        if (w == 0) return (adv ? adv : VARFONT_DEFAULT_SPACE_ADVANCE);
        // else draw like normal below by using c=' ' (idx known). Fall through.
    }

    const int16_t idx = VarFont_FindGlyphIndex(font, cp);
    if (idx < 0) return 0;

    const uint8_t width   = pgm_read_byte(&font->widths[idx]);
    const uint8_t advance = pgm_read_byte(&font->advances[idx]);

    // No ink: do not draw, but still advance.
    if (width == 0) {
        if (cp == 32 && advance == 0) return VARFONT_DEFAULT_SPACE_ADVANCE;
        return (advance != 0) ? advance : 0;
    }

    const uint32_t offset = pgm_read_dword(&font->offsets[idx]);

    for (uint8_t row = 0; row < font->Height; row++) {
        const uint8_t hi = pgm_read_byte(&font->table[offset + (uint32_t)row * 2u]);
        const uint8_t lo = pgm_read_byte(&font->table[offset + (uint32_t)row * 2u + 1u]);
        const uint16_t bits = (uint16_t)((hi << 8) | lo);

        for (uint8_t col = 0; col < width; col++) {
            if (bits & (uint16_t)(0x8000u >> col)) {
                if (draw) {
                    paint.DrawPixel(x + col, y + row, color);
                }
            }
        }
    }

    // If advance is 0 (shouldn't happen), fall back to width.
    return (advance != 0) ? advance : width;
}

uint16_t DrawVarString(int x, int y, const char* text, const sVARFONT* font, uint16_t color, uint8_t spacing, bool draw, Paint& paint)
{
    int cursor_x = x;

    while (text && *text) {
        cursor_x += DrawVarChar(cursor_x, y, *text, font, color, draw, paint);
        if (*(text + 1)) cursor_x += spacing;
        text++;
    }

    return (uint16_t)(cursor_x - x);
}

uint16_t MeasureVarString(const char* text, const sVARFONT* font, uint8_t spacing)
{
    uint16_t w = 0;

    while (text && *text) {
        w += VarFont_GetCharAdvance(font, *text);
        if (*(text + 1)) w += spacing;
        text++;
    }

    return w;
}
