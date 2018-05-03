/*
 * fonts.h
 *
 * Шрифты
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef FONTS_H_
#define FONTS_H_

#include <avr/io.h>
#include <avr/pgmspace.h>

extern PROGMEM uint8_t const font_normal16[];

extern PROGMEM uint8_t const font_robot16[];

// Количество шрифтов в массиве fonts_list
#define FONTS_COUNT 3

extern PROGMEM PGM_VOID_P const fonts_list[];


#endif /* FONTS_H_ */