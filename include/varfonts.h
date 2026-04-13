#include <stdint.h>
#include <stdbool.h>
#include <pgmspace.h>
#include "epdpaint.h"

// Sparse variable-width bitmap font (only stores glyphs that exist).
//
// Bitmap format per glyph:
//   - Height rows
//   - Each row is 16-bit MSB-first (bit15 is leftmost pixel), stored as 2 bytes (hi, lo)
//
// Lookup:
//   - `codepoints[]` is sorted. We binary-search it for a codepoint.
//   - `offsets[i]` is the byte offset into `table[]` for glyph i.
//
// Metrics:
//   - `widths[i]`   = ink width in pixels (columns to draw)
//   - `advances[i]` = cursor advance in pixels (xAdvance)

typedef struct sVARFONT
{
  const uint8_t  *table;       // PROGMEM bitmap bytes
  const uint16_t *codepoints;  // PROGMEM sorted codepoints
  const uint32_t *offsets;     // PROGMEM byte offsets into table
  const uint8_t  *widths;      // PROGMEM ink widths
  const uint8_t  *advances;    // PROGMEM cursor advances
  uint16_t glyph_count;        // number of glyphs
  uint8_t  Height;             // glyph height (rows)
  uint8_t  max_width;          // max ink width (informational)
} sVARFONT;

// ---- Metric helpers (ASCII char -> Unicode codepoint via unsigned byte) ----
uint8_t  VarFont_GetCharWidth(const sVARFONT* font, char c);
uint8_t  VarFont_GetCharAdvance(const sVARFONT* font, char c);
uint32_t VarFont_GetCharOffset(const sVARFONT* font, char c);

// ---- Drawing / Measuring ----
uint8_t  DrawVarChar(int x, int y, char c, const sVARFONT* font, uint16_t color, bool draw, Paint& paint);
uint16_t DrawVarString(int x, int y, const char* text, const sVARFONT* font, uint16_t color, uint8_t spacing, bool draw, Paint& paint);
uint16_t MeasureVarString(const char* text, const sVARFONT* font, uint8_t spacing);

// Your existing fonts
extern sVARFONT OpenSansPX;
extern sVARFONT Born2bSporty;
extern sVARFONT BoldPixels;