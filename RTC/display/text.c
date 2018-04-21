/*
 * text.c
 *
 * Модуль для отображения текстовой информации
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#include "text.h"
#include "display.h"



#define _TEXT_FLAG_INVERTED 1
#define _TEXT_FLAG_2X_HORIZONTAL 2
#define _TEXT_FLAG_2X_VERTICAL 4

static PGM_VOID_P text_font;
static uint8_t text_flags;
static uint8_t text_x, text_page;
static uint8_t text_limit_x = DISPLAY_WIDTH;


void text_reset() {
  text_x = 0;
  text_page = 0;
  text_flags = 0;
  text_limit_x = DISPLAY_WIDTH;
}

void text_select_font(PGM_VOID_P font) {
  text_font = font;
}

void text_set_pos(uint8_t page, uint8_t x) {
  text_page = page;
  text_x = x;
}

uint8_t text_set_limit_x(uint8_t limit_x) {
  uint8_t prev = text_limit_x;
  text_limit_x = (limit_x >= DISPLAY_WIDTH) ? DISPLAY_WIDTH : limit_x;
  return prev;
}

static uint8_t _text_display_font_data(uint8_t page, uint8_t x, uint8_t width, PGM_VOID_P pgmData, uint8_t step, uint8_t fontFlags) {
  if ((x >= text_limit_x) || (page >= DISPLAY_PAGES))
    return x;
  uint8_t xorMask = (text_flags & _TEXT_FLAG_INVERTED) ? 0xFF : 0;
  uint8_t horiz2x = text_flags & _TEXT_FLAG_2X_HORIZONTAL;
  if (!display_setpos(page, x)) return 0;
  if (fontFlags & FONT_FLAG_BLANK_BEFORE) {
    if (!display_data(xorMask)) return 0;
    if (++x >= text_limit_x)
      return x;
    if (horiz2x) {
      if (!display_data(xorMask)) return 0;
      if (++x >= text_limit_x)
        return x;
    }
  }
  while (width--) {
    uint8_t d = pgm_read_byte(pgmData) ^ xorMask;
    if (!display_data(d)) return 0;
    if (++x >= text_limit_x)
      return x;
    if (horiz2x) {
      if (!display_data(d)) return 0;
      if (++x >= text_limit_x)
        return x;
    }
    pgmData += step;
  }
  if (fontFlags & FONT_FLAG_BLANK_AFTER) {
    if (!display_data(xorMask)) return 0;
    if (horiz2x) {
      if (++x >= text_limit_x)
        return x;
      if (!display_data(xorMask)) return 0;
    }
    x++;
  }
  return x;
}

static uint8_t _text_display_font_data2x(uint8_t page, uint8_t x, uint8_t width, PGM_VOID_P pgmData, uint8_t step, uint8_t fontFlags, uint8_t isLowerHalf) {
  if ((x >= text_limit_x) || (page >= DISPLAY_PAGES))
    return x;
  uint8_t xorMask = (text_flags & _TEXT_FLAG_INVERTED) ? 0xFF : 0;
  uint8_t horiz2x = text_flags & _TEXT_FLAG_2X_HORIZONTAL;
  display_setpos(page, x);
  if (fontFlags & FONT_FLAG_BLANK_BEFORE) {
    if (!display_data(xorMask)) return 0;
    if (++x >= text_limit_x) return x;
    if (horiz2x) {
      if (!display_data(xorMask)) return 0;
      if (++x >= text_limit_x) return x;
    }    
  }
  while (width--) {
    uint8_t dt = pgm_read_byte(pgmData);
    if (isLowerHalf) dt = dt >> 4;
    
    dt = ((dt & 1) | ((dt & 1) << 1) | ((dt & 2) << 1) | ((dt & 2) << 2) | ((dt & 4) << 2) | ((dt & 4) << 3) | ((dt & 8) << 3) | ((dt & 8) << 4)) ^ xorMask;
    display_data(dt);
    if (++x >= text_limit_x) return x;
    if (horiz2x) {
      display_data(dt);
      if (++x >= text_limit_x) return x;
    }    
    pgmData += step;
  }
  if (fontFlags & FONT_FLAG_BLANK_AFTER) {
    display_data(xorMask);
    if (horiz2x) {
      if (++x >= text_limit_x) return x;
      display_data(xorMask);
    }     
    x++;
  }
  return x;
}

uint8_t text_char(uint8_t code) {
  uint8_t x = text_x;
  if (x >= text_limit_x) return 1;
  uint8_t page = text_page;
  if (page >= DISPLAY_PAGES) return 1;
  
  PGM_VOID_P p = text_font;
  if (!p) return 1;
  uint8_t fontFlags = pgm_read_byte(p++);
  uint8_t fontSize = pgm_read_byte(p++);
  uint8_t fontStart = pgm_read_byte(p++);
  uint8_t fontEnd = pgm_read_byte(p++);
  if ((code < fontStart) | (code > fontEnd)) return 1;
    
  p += (code - fontStart) * 2;
  
  uint16_t offset = pgm_read_byte(p++);
  uint8_t chwidth = pgm_read_byte(p);
  offset |= (chwidth & 0x0F) << 8;
  chwidth >>= 4;
  
  p = text_font + offset;
  if (text_flags & _TEXT_FLAG_2X_VERTICAL) {
    if (fontSize & 8) { // 16-пиксельный шрифт
      uint8_t nx = _text_display_font_data2x(page, x, chwidth, p, 2, fontFlags, 0);
      if (!nx) return 0;
      if (++page >= DISPLAY_PAGES) return 1;
      if (!_text_display_font_data2x(page, x, chwidth, p, 2, fontFlags, 1)) return 0;
      if (++page >= DISPLAY_PAGES) return 1;
      if (!_text_display_font_data2x(page, x, chwidth, p + 1, 2, fontFlags, 0)) return 0;
      if (++page >= DISPLAY_PAGES) return 1;
      if (!_text_display_font_data2x(page, x, chwidth, p + 1, 2, fontFlags, 1)) return 0;
      text_x = nx;
    }  else {
      uint8_t nx = _text_display_font_data2x(page, x, chwidth, p, 1, fontFlags, 0);
      if (!nx) return 0;
      if (++page >= DISPLAY_PAGES) return 1;
      if (!_text_display_font_data2x(page, x, chwidth, p, 1, fontFlags, 0)) return 0;
      text_x = nx;
    }  
  } else {
    if (fontSize & 8) { // 16-пиксельный шрифт
      uint8_t nx = _text_display_font_data(page, x, chwidth, p, 2, fontFlags);
      if (!nx) return 0;
      if (++page >= DISPLAY_PAGES) return 1;
      if (!_text_display_font_data(page, x, chwidth, p + 1, 2, fontFlags)) return 0;
      text_x = nx;
    }  else {
      uint8_t nx = _text_display_font_data(page, x, chwidth, p, 1, fontFlags);
      if (!nx) return 0;
      text_x = nx;
    }  
  }
  return 1;
}

uint8_t text_char_width(uint8_t code) {
  PGM_VOID_P p = text_font;
  if (!p) return 0;
  uint8_t fontFlags = pgm_read_byte(p);
  p += 2; // fontSize
  uint8_t fontStart = pgm_read_byte(p++);
  uint8_t fontEnd = pgm_read_byte(p++);
  if ((code < fontStart) | (code > fontEnd)) return 1;
  if ((code < fontStart) | (code > fontEnd)) return 1;
    
  p += (code - fontStart) * 2 + 1;
  
  uint8_t chwidth = pgm_read_byte(p);
  chwidth >>= 4;
  
  if (fontFlags & FONT_FLAG_BLANK_BEFORE) chwidth++;
  if (fontFlags & FONT_FLAG_BLANK_AFTER) chwidth++;
  if (text_flags & _TEXT_FLAG_2X_HORIZONTAL) chwidth <<= 1;

  return chwidth;
}  



void text_set_2x(uint8_t horizontal2x, uint8_t vertical2x) {
  text_flags &= ~(_TEXT_FLAG_2X_HORIZONTAL | _TEXT_FLAG_2X_VERTICAL);
  if (horizontal2x) text_flags |= _TEXT_FLAG_2X_HORIZONTAL;
  if (vertical2x) text_flags |= _TEXT_FLAG_2X_VERTICAL;
}

void text_set_inversion(uint8_t inverted) {
  text_flags &= ~_TEXT_FLAG_INVERTED;
  if (inverted) text_flags |= _TEXT_FLAG_INVERTED;
}

uint8_t text_blank(uint8_t width) {
  uint8_t x = text_x;
  uint8_t page = text_page;
  if (x >= text_limit_x) return 1;
  if (width > (text_limit_x - x)) width = (text_limit_x - x);
  PGM_VOID_P p = text_font;
  uint8_t fontSize = pgm_read_byte(p + 1);
  uint8_t ln = ((fontSize & 8) ? 2 : 1);
  if (text_flags & _TEXT_FLAG_2X_VERTICAL) ln <<= 1;
  uint8_t xorMask = (text_flags & _TEXT_FLAG_INVERTED) ? 0xFF : 0;
  while (ln-- && (page < DISPLAY_PAGES)) {
    if (!display_setpos(page, x)) return 0;
    for (uint8_t i = 0; i < width; i++) {
      if (!display_data(xorMask)) return 0;
    }
    page++;
  }
  text_x = x + width;
  return 1;
}

uint8_t text_pad(uint8_t toX) {
  if (toX > text_limit_x) toX = text_limit_x;
  if (text_x >= toX) return 1;
  return text_blank(toX - text_x);
}

uint8_t text_print(PGM_P pgmStr) {
  if (!pgmStr) return 0;
  while (text_x < text_limit_x)  {
    uint8_t ch = pgm_read_byte(pgmStr++);
    if (ch == 0) break;
    if (!text_char(ch)) return 0;
  }
  return 1;
}

uint16_t text_width(PGM_P pgmStr) {
  if (!pgmStr) return 0;
  uint16_t w = 0;
  for(;;)  {
    uint8_t ch = pgm_read_byte(pgmStr++);
    if (ch == 0) return w;
    w += text_char_width(ch);
  }
}

static PROGMEM uint32_t const _text_num_scalers[] = {1, 10, 100, 1000, 10000, 100000UL, 1000000UL, 10000000UL, 100000000UL, 1000000000UL};

static uint16_t _text_num(uint32_t num, uint8_t decpoint, uint8_t calcwidth) {
  if (!calcwidth && ((text_x >= text_limit_x) || (text_page >= DISPLAY_PAGES) || !text_font)) return 1;
  uint8_t outme = 0;
  uint16_t width = 0;
  for (int8_t d = (sizeof(_text_num_scalers) / sizeof(_text_num_scalers[0])) - 1; d >= 0; d--) {
    uint8_t r = 0;
    uint32_t scaler = pgm_read_dword(&_text_num_scalers[d]);
    while (num >= scaler) {
      r++;
      num -= scaler;
    }
    if ((r > 0) || (decpoint >= d)) 
      outme = 1;
    if (calcwidth) {
      if (outme) width += text_char_width(r + 48);
      if (decpoint && (decpoint == d)) width += text_char_width('.');
    } else {
      if (outme && !text_char(r + 48)) return 0;
      if (decpoint && (decpoint == d) && !text_char('.')) return 0;
    }    
  }
  return calcwidth ? width : 1;
}

uint8_t text_unum(uint32_t num, uint8_t decpoint) {
  return _text_num(num, decpoint, 0);
}  


uint16_t text_unum_width(uint32_t num, uint8_t decpoint) {
  return _text_num(num, decpoint, 1);
}

uint8_t text_snum(int32_t num, uint8_t decpoint) {
  if (num < 0) {
    if (!text_char('-')) return 0;
    num = -num;
  }  
  return _text_num(num, decpoint, 0);
}

uint16_t text_snum_width(int32_t num, uint8_t decpoint) {
  uint16_t w = 0;
  if (num < 0) {
    w += text_char_width('-');
    num = -num;
  }  
  w += _text_num(num, decpoint, 1);
  return w;
}

