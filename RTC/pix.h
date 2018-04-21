/*
 * pix.h
 *
 * Константы для отрисовки пиктограммок (сами данные находятся в генерируемом generated/pix_data.c)
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef PIX_H_
#define PIX_H_

#define PIX_WIDTH 16
#define PIX_HEIGHT_PAGES 2

#define PIX_NEXT 0
#define PIX_RETURN 1
#define PIX_ENTER 2
#define PIX_OK 3
#define PIX_CANCEL 4
#define PIX_INCREASE 5
#define PIX_DECREASE 6
#define PIX_SAVE 7
#define PIX_CALIBRATE 8
#define PIX_EMPTY 255

extern PROGMEM uint8_t const pix_data[][2][16];

#endif /* PIX_H_ */