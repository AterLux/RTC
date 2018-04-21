/*
 * display.c
 *
 * Модуль работы с монохромными дисплеями
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include "display.h"


/* Заполняет область заданными чередующимися значениями */
uint8_t display_fill_region(uint8_t page, uint8_t x, uint8_t pages_height, uint8_t width, uint8_t evenData, uint8_t oddData) {
  if ((x >= DISPLAY_WIDTH) || (page >= DISPLAY_PAGES)) return 1;
  uint8_t w = DISPLAY_WIDTH - x;
  if (w > width)
    w = width;
  uint8_t t = (pages_height > (DISPLAY_PAGES - page)) ? DISPLAY_PAGES : (page + pages_height);
  for (uint8_t p = page ; p < t; p++) {
    if (!display_setpos(p, x)) return 0;
    uint8_t c = w;
    if (x & 1) {
      if (!display_data(oddData)) return 0;
      c--;
    }
    while (c >= 2) {
      if (!display_data(evenData) || !display_data(oddData)) return 0;
      c -=2;
    }
    if (c && !display_data(evenData)) return 0;
  }
  return 1;
}

/* Заполняет экран заданными чередующимися значениями */
uint8_t display_fill(uint8_t evenData, uint8_t oddData) {
  return display_fill_region(0, 0, DISPLAY_PAGES, DISPLAY_WIDTH, evenData, oddData);
}

/* Очищает экран, устанавливает курсор в левый верхний угол */
uint8_t display_clear() {
  return display_fill(0, 0) && display_setpos(0, 0);
}

/* Выводит на экран изображение из флеш-памяти
 * page, x - начальная позиция для вывода буфера
 * pData - указатель на изображение во флеш-памати
 * pages_height - высота изоюражения (в страницах)
 * width - ширина изображения
 */
uint8_t display_sprite(uint8_t page, uint8_t x, PGM_VOID_P pData, uint8_t pages_height, uint8_t width) {
  if ((page >= DISPLAY_PAGES) || (x >= DISPLAY_WIDTH))
    return 1;
  uint8_t w = DISPLAY_WIDTH - x;
  if (w > width)
    w = width;
  uint8_t t = (pages_height > (DISPLAY_PAGES - page)) ? DISPLAY_PAGES : (page + pages_height);
  while (page < t) {
    if (!display_setpos(page++, x)) return 0;
    for (uint8_t i = 0; i < w; i++) {
      if (!display_data(pgm_read_byte(pData + i))) return 0;
    }
    pData += width;
  }
  return 1;
}

/* Копирует из буфера изображение на экран
 * page, x - начальная позиция для вывода буфера
 * pData - указатель на буфер
 * pages_height - высота буфера (в страницах)
 * width - ширина буфера
 */
uint8_t display_buffer(uint8_t page, uint8_t x, uint8_t * pData, uint8_t pages_height, uint8_t width) {
  if ((x >= DISPLAY_WIDTH) || (page >= DISPLAY_PAGES))
    return 1;
  uint8_t w = DISPLAY_WIDTH - x;
  if (w > width)
    w = width;
  uint8_t t = (pages_height > (DISPLAY_PAGES - page)) ? DISPLAY_PAGES : (page + pages_height);
  while (page < t) {
    if (!display_setpos(page++, x)) return 0;
    for (uint8_t i = 0; i < w; i++) {
      if (!display_data(pData[i])) return 0;
    }
    pData += width;
  }
  return 1;
}
