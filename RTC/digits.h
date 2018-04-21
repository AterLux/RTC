/*
 * digits.h
 *
 * Модуль для отрисовки цифр часов и анимации переходов
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef DIGITS_H_
#define DIGITS_H_

#include <avr/io.h>

#define DIGIT_EMPTY 255
#define DIGIT_TIME_SEPARATOR 10
#define DIGIT_DATE_SEPARATOR 11

#define DIGIT_WIDTH 16


#define DIGITS_ANIMATION_NONE 0
#define DIGITS_ANIMATION_ROTATE 1
#define DIGITS_ANIMATION_FLIP 2
#define DIGITS_ANIMATION_ROLL 3
#define DIGITS_ANIMATION_MAGIC 4

#define DIGITS_ANIMATIONS_COUNT 5

/* Выбирает начертание цифр */
void digits_select_font(uint8_t font);

/* Возвращает код текущего начертания цифр */
uint8_t digits_current_font();

/* Выводит указанную цифру в указанной горизонтальной позиции */
uint8_t digit_output(uint8_t d, uint8_t x);

/* Помещает две последние десятичные цифры числа num в два байта, начиная с p_buf.
 * Если leading_zero == 0, то для чисел меньше 10 на первой позиции  помещает DIGIT_EMPTY*/
uint8_t two_digits(uint8_t num, uint8_t * p_buf, uint8_t leading_zero);

/* Возвращает код текущего эффекта перехода */
uint8_t digits_get_animation();

/* Выбирает анимационный эффект перехода */
void digits_set_animation(uint8_t animation);

/* Отрисовывает на указанном месте один кадр перехода из цифры d_from в d_to. Pos32th - от 0 до 31: указывает в 1/32 долях насколько завершена анимания */
uint8_t digit_transition(uint8_t d_from, uint8_t d_to, uint8_t x, uint8_t pos32th);







#endif /* DIGITS_H_ */