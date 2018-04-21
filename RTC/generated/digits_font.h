// This file is generated automatically
// Source file timestamp 15.4.2018 14:06:51
// Generated at 15.4.2018 14:06:53

#ifndef DIGITS_FONT_H_
#define DIGITS_FONT_H_

#include <avr/io.h>
#include <avr/pgmspace.h>

#define DIGITS_COLS_PER_BLOCK 16

#define DIGITS_BLOCKS_PER_SYMBOL 4

#define DIGITS_SYMBOLS_PER_FONT 12

#define DIGITS_FONTS_COUNT 4

#define DIGITS_BLOCKS_COUNT 109

extern PROGMEM uint8_t const digits_datablocks[DIGITS_BLOCKS_COUNT][DIGITS_COLS_PER_BLOCK];

extern PROGMEM uint8_t const digits_fonts[DIGITS_FONTS_COUNT][DIGITS_SYMBOLS_PER_FONT][DIGITS_BLOCKS_PER_SYMBOL];

#endif /* DIGITS_FONT_H_ */

