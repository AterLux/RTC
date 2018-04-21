/*
 * digits.c
 *
 * Модуль для отрисовки цифр часов и анимации переходов
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include "digits.h"
#include "generated/digits_font.h"
#include "display/display.h"
#include <avr/pgmspace.h>

static uint8_t digits_font;
static uint8_t digits_animation = DIGITS_ANIMATION_FLIP;


void digits_select_font(uint8_t font) {
  if (font < DIGITS_FONTS_COUNT) {
    digits_font = font;
  }
}

uint8_t digits_current_font() {
  return digits_font;
}

uint8_t digit_output(uint8_t d, uint8_t x) {
  if (x >= DISPLAY_WIDTH) return 1;
  if (d >= DIGITS_SYMBOLS_PER_FONT) {
    return display_fill_region(0, x, DIGITS_BLOCKS_PER_SYMBOL, DIGITS_COLS_PER_BLOCK, 0, 0);
  }
  PGM_VOID_P pblocks = &digits_fonts[digits_font][d];
  uint8_t w = (x > (DISPLAY_WIDTH - DIGITS_COLS_PER_BLOCK)) ? (DISPLAY_WIDTH - x) : DIGITS_COLS_PER_BLOCK;
  for (uint8_t p = 0 ; p < DIGITS_BLOCKS_PER_SYMBOL ; p++) {
    if (!display_setpos(p, x)) return 0;
    PGM_VOID_P pdata = &digits_datablocks[pgm_read_byte(pblocks++)];
    for (uint8_t i = w ; i > 0 ; i--) {
      if (!display_data(pgm_read_byte(pdata++))) return 0;
    }      
  }
  return 1;
}

uint8_t two_digits(uint8_t num, uint8_t * p_buf, uint8_t leading_zero) {
  uint8_t r = 0;
  while (num >= 100) {
    num -= 100;
    r++;
  }
  uint8_t d = 0;
  while (num >= 10) {
    num -= 10;
    d++;
  }
  if ((d == 0) && !leading_zero) d = DIGIT_EMPTY;
  *(p_buf++) = d;
  *p_buf = num;
  return r;
}

/*
uint8_t output_shrinked(uint8_t d, uint8_t x, uint8_t page) {
  if ((x >= DISPLAY_WIDTH) || (page >= DISPLAY_PAGES)) return 1;
  if (d >= DIGITS_SYMBOLS_PER_FONT) {
    return display_fill_region(page, x, DIGITS_BLOCKS_PER_SYMBOL / 2, DIGITS_COLS_PER_BLOCK / 2, 0, 0);
  }
  PGM_VOID_P pblocks = &digits_fonts[digits_font][d];
  for (uint8_t p = 0 ; p < (DIGITS_BLOCKS_PER_SYMBOL / 2) ; p++) {
    PGM_VOID_P pdatau = &digits_datablocks[pgm_read_byte(pblocks++)];
    PGM_VOID_P pdatad = &digits_datablocks[pgm_read_byte(pblocks++)];
    if (!display_setpos(page + p, x)) return 0;
    for (uint8_t i = 0 ; i < (DIGITS_COLS_PER_BLOCK / 2) ; i++) {
      uint16_t dl = pgm_read_byte(pdatau++) | (pgm_read_byte(pdatad++) << 8);
      uint16_t dr = pgm_read_byte(pdatau++) | (pgm_read_byte(pdatad++) << 8);
      uint8_t d = 0;
      for (uint8_t b = 0 ; b < 8 ; b++) {
        d >>= 1;
        uint8_t c = ((uint8_t)dl & 1) + ((uint8_t)dr & 1) + (((uint8_t)dl >> 1) & 1) + (((uint8_t)dr >> 1) & 1);
        if (c > 2) d |= 0x80;
        dl >>= 2;
        dr >>= 2;
      }
      if (!display_data(d)) return 0;
    }
  }
  return 1;
}
*/


uint8_t digits_get_animation() {
  return digits_animation;
}

void digits_set_animation(uint8_t animation) {
  digits_animation = animation;
}

// Полупериод синуса - выбирает ускорение и замедление анимации, чтобы она не выглядела угловато
PROGMEM uint8_t const harmonic_table[32] = 
   {   0,   1,   2,   6,  10,  15,  22,  29,  37,  47,  57,  68,  79,  91, 103, 115, 
     128, 141, 153, 165, 177, 188, 199, 209, 219, 227, 234, 241, 246, 250, 254, 255 };

// Эффект "проворачивания" цифры вокруг вертикальной оси: старая цифра сжимается горизонтально к левому краю, справа расправляется новая цифра
static uint8_t digit_transition_rotate(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th) {
  if (pos32th == 0) return 1;
  uint8_t r_w = pgm_read_byte(&harmonic_table[pos32th]);
  PGM_VOID_P pblocks_from = (d_from < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_from] : 0;
  PGM_VOID_P pblocks_to = (d_to < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_to] : 0;
  uint8_t w = (x > (DISPLAY_WIDTH - DIGITS_COLS_PER_BLOCK)) ? (DISPLAY_WIDTH - x) : DIGITS_COLS_PER_BLOCK;
  uint16_t accum[8];
  for (uint8_t p = 0 ; p < DIGITS_BLOCKS_PER_SYMBOL ; p++) {
    if (!display_setpos(p, x)) return 0;
    for (uint8_t i = 0 ; i < 8 ; i++) accum[i] = 0;
    
    PGM_VOID_P pdata;
    if (pblocks_from) {
      pdata = &digits_datablocks[pgm_read_byte(pblocks_from++)];
    } else {
      pdata = 0;
    }      
    
    uint8_t reading_right = 0;
    uint8_t src_wrap = 256 - r_w;
    uint8_t dest_x = 0;
    uint8_t src_x = 0;
    uint8_t dest_acc = 0;
    uint8_t src_rem = 0;
    uint8_t data = 0;
    for(;;) {
      if (src_rem == 0) { 
        if (src_x >= DIGITS_COLS_PER_BLOCK) {
          if (reading_right) {
            break;
          } else {
            pdata = pblocks_to ? &digits_datablocks[pgm_read_byte(pblocks_to)] : 0;
            src_wrap = r_w;
            src_x = 0;
          }
        } 
        if (pdata) {
          data =  pgm_read_byte(pdata++);
        } else {
          data = 0;
        }                   
        src_x++;
        src_rem = src_wrap;
      } 
      uint8_t cut = src_rem;
      if (cut > (256 - dest_acc)) {
        cut = 256 - dest_acc;
      }
      src_rem -= cut;
      
      uint8_t msk = 1;
      for (uint8_t i = 0 ; i < 8 ; i++) {
        if (data & msk) {
          accum[i] += cut;
        }
        msk <<= 1;
      }
      if ((cut + dest_acc) >= 256) {
        uint8_t d = 0;
        for (uint8_t i = 0 ; i < 8 ; i++) {
          d >>= 1;
          if (accum[i] >= 128) d |= 0x80;
          accum[i] = 0;
        }    
        if (!display_data(d)) return 0;
        dest_x++;
        if (dest_x >= w) break;
        dest_acc = 0;
      } else {
        dest_acc += cut;
      }
    }
    if (pblocks_to) pblocks_to++;
  }   
  return 1; 
}

// Эффект возникновения. Пиксели старой цифры случайно заменяются на пиксели новой. Верхние пиксели заменяются несколько раньше, чем нижние
static uint8_t digit_transition_magic(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th) {
  uint8_t rmin = pos32th << 3;
  uint16_t rnd = d_from * 26557 + d_to * 22157;
  PGM_VOID_P pblocks_from = (d_from < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_from] : 0;
  PGM_VOID_P pblocks_to = (d_to < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_to] : 0;
  uint8_t w = (x > (DISPLAY_WIDTH - DIGITS_COLS_PER_BLOCK)) ? (DISPLAY_WIDTH - x) : DIGITS_COLS_PER_BLOCK;
  for (uint8_t p = 0 ; p < DIGITS_BLOCKS_PER_SYMBOL ; p++) {
    if (!display_setpos(p, x)) return 0;
    PGM_VOID_P pdata_from;
    if (pblocks_from) {
      pdata_from = &digits_datablocks[pgm_read_byte(pblocks_from++)];
    } else {
      pdata_from = 0;
    }    
    PGM_VOID_P pdata_to;
    if (pblocks_to) {
      pdata_to = &digits_datablocks[pgm_read_byte(pblocks_to++)];
    } else {
      pdata_to = 0;
    }
    for (uint8_t dx = 0 ; dx < w ; dx++) {
      uint8_t data_from;
      if (pdata_from) {
        data_from = pgm_read_byte(pdata_from++);
      } else {
        data_from = 0;
      }      
      uint8_t data_to;
      if (pdata_to) {
        data_to = pgm_read_byte(pdata_to++);
      } else {
        data_to = 0;
      }        
      uint8_t radd = p * 16;
      uint8_t msk = 1;
      uint8_t d = 0;
      for (uint8_t i = 0 ; i < 8 ; i++ ) {
        rnd = (uint16_t)(rnd + 1) * 53093U;
        d |=  (((uint8_t)(((uint8_t)(rnd >> 10U) * 3) + radd) >= rmin) ? data_from : data_to) & msk;
        radd += 2;
        msk <<= 1;
      }
      if (!display_data(d)) return 0;
    }    
  }    
  return 1;
}  

// Эффект прокатывания. Старая цифра уезжает вверх, под ней снизу выезжает новая цифра
static uint8_t digit_transition_roll(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th) {
  if (pos32th == 0) return 1;
  uint8_t y_off = (pgm_read_byte(&harmonic_table[pos32th]) + 4) >> 3;

  uint8_t blk = (y_off >> 3);
  uint8_t b_shift = y_off & 7;
  
  PGM_VOID_P pblocks;
  if (blk >= 4) {
    blk = 0;
    pblocks = (d_to < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_to] : 0;
  } else {
    pblocks = (d_from < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_from][blk] : 0;
  } 
  uint8_t w = (x > (DISPLAY_WIDTH - DIGITS_COLS_PER_BLOCK)) ? (DISPLAY_WIDTH - x) : DIGITS_COLS_PER_BLOCK;
  for (uint8_t p = 0 ; p < DIGITS_BLOCKS_PER_SYMBOL ; p++) {
    PGM_VOID_P p_data_up;
    if (pblocks) {
      p_data_up = &digits_datablocks[pgm_read_byte(pblocks++)];
    } else {
      p_data_up = 0;
    };
    if (++blk >= 4) {
      blk = 0;
      pblocks = (d_to < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_to] : 0;
    };
    PGM_VOID_P p_data_down = pblocks ? &digits_datablocks[pgm_read_byte(pblocks)] : 0;
    
    if (!display_setpos(p, x)) return 0;
    
    for (uint8_t dx = 0 ; dx < w ; dx++) {
      uint8_t d;
      if (p_data_up) {
        d = pgm_read_byte(p_data_up++);
      } else {
        d = 0;
      }      
      if (b_shift > 0) {
        d >>= b_shift;
        if (p_data_down) {
          d |= (pgm_read_byte(p_data_down++) << (8 - b_shift));
        }        
      }
      if (!display_data(d)) return 0;
    }
  }   
  return 1; 
}  

// Эффект "перелистывания календарика": верхняя половина старой цифры сжимается к середине, открывая новую цифру, затем в нижней половине от середины раскрывается новая цифра, затирая старую
static uint8_t digit_transition_flip(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th) {
  if (pos32th == 0) return 1;
  uint8_t y_flip = (pgm_read_byte(&harmonic_table[pos32th]) + 4) >> 3;

  const uint8_t * pblocks_from = (d_from < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_from][0] : 0;
  const uint8_t * pblocks_to = (d_to < DIGITS_SYMBOLS_PER_FONT) ? &digits_fonts[digits_font][d_to][0] : 0;
  uint8_t w = (x > (DISPLAY_WIDTH - DIGITS_COLS_PER_BLOCK)) ? (DISPLAY_WIDTH - x) : DIGITS_COLS_PER_BLOCK;
  for (uint8_t p = 0 ; p < 4 ; p++) {
    if (!display_setpos(p, x)) return 0;
    PGM_VOID_P p_data_straight;
    PGM_VOID_P p_data_fl_u = 0;
    PGM_VOID_P p_data_fl_d = 0;
    uint8_t flipped_y_start = 0, flipped_y_end = 8;
    uint8_t shrink_factor = 0;
    uint8_t top_flip_pixel = 0;
    uint8_t top_flip_subpixel = 0;
    if (p < 2) {
      p_data_straight = pblocks_to ? &digits_datablocks[pgm_read_byte(&pblocks_to[p])] : 0;
      if (y_flip >= 16) {
        flipped_y_end = 0;
      } else {      
        shrink_factor = 16 - y_flip;
        if (pblocks_from) {
          p_data_fl_u = &digits_datablocks[pgm_read_byte(&pblocks_from[0])];
          p_data_fl_d = &digits_datablocks[pgm_read_byte(&pblocks_from[1])];
        }
        if (p == 0) {
          flipped_y_start = y_flip;
        } else {
          if (y_flip < 8) {
            flipped_y_start = 0;
            top_flip_subpixel = (8 - y_flip) * 16;
            while (top_flip_subpixel >= shrink_factor) {
              top_flip_subpixel -= shrink_factor;
              top_flip_pixel++;
            }
          } else {
            flipped_y_start =  y_flip - 8;
          }
        }
      }      
    } else {
      p_data_straight = pblocks_from ? &digits_datablocks[pgm_read_byte(&pblocks_from[p])] : 0;
      if (y_flip <= 16) {
        flipped_y_end = 0;
      } else {
        shrink_factor = y_flip - 16;
        if (pblocks_to) {
          p_data_fl_u = &digits_datablocks[pgm_read_byte(&pblocks_to[2])];
          p_data_fl_d = &digits_datablocks[pgm_read_byte(&pblocks_to[3])];
        }
        if (p == 2) {
          flipped_y_end = y_flip - 16;
        } else {
          if (y_flip <= 24) {
            flipped_y_end = 0;
          } else {
            top_flip_subpixel = 128;
            while (top_flip_subpixel >= shrink_factor) {
              top_flip_subpixel -= shrink_factor;
              top_flip_pixel++;
            }
            flipped_y_end = y_flip - 24;
          }            
        }
      }        
    }
    if (flipped_y_end <= flipped_y_start) {
      for (uint8_t dx = 0 ; dx < w ; dx++) {
        uint8_t d;
        if (p_data_straight) {
          d = pgm_read_byte(p_data_straight++);
        } else {
          d = 0;
        }
        if (!display_data(d)) return 0;
      }        
    } else {      
      for (uint8_t dx = 0 ; dx < w ; dx++) {
        uint8_t d;
        if (p_data_straight) {
          d = pgm_read_byte(p_data_straight++);
        } else {
          d = 0;
        }
        uint16_t fld ;
        if (p_data_fl_u) {
          fld = pgm_read_byte(p_data_fl_u++) | (pgm_read_byte(p_data_fl_d++) << 8); 
        } else {
          fld = 0;
        }
        for (uint8_t i = top_flip_pixel ; i != 0 ; i--) fld >>= 1;
        uint8_t msk = 1;
        uint8_t in_y = flipped_y_start;
        for (uint8_t i = in_y ; i != 0 ; i--) msk <<= 1;
        uint8_t sub_rem = 16 - top_flip_subpixel;
        uint8_t acc = 0;
        uint8_t filled = 0;
        do {
          if (sub_rem == 0) {
            fld >>= 1;
            sub_rem = shrink_factor;
          }
          uint8_t inc = 16 - filled;
          if (inc > sub_rem) inc = sub_rem;
          if (fld & 1) acc += inc;
          sub_rem -= inc;
          filled += inc;
          if (filled >= 16) {
            if (acc >= 8) {
              d |= msk;
            } else {
              d &= ~msk;
            }
            msk <<= 1;
            acc = 0;
            filled = 0;
            in_y++;
          }
        } while (in_y < flipped_y_end);
        if (!display_data(d)) return 0;
      }        
    }    
  }   
  return 1; 
} 


uint8_t digit_transition(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th) {
  switch(digits_animation) {
    case DIGITS_ANIMATION_ROTATE: 
      return digit_transition_rotate(d_from, d_to, x, pos32th); 
    case DIGITS_ANIMATION_ROLL: 
      return digit_transition_roll(d_from, d_to, x, pos32th); 
    case DIGITS_ANIMATION_FLIP: 
      return digit_transition_flip(d_from, d_to, x, pos32th); 
    case DIGITS_ANIMATION_MAGIC:
      return digit_transition_magic(d_from, d_to, x, pos32th);
    default:
      return digit_output(d_to, x);
  }
}