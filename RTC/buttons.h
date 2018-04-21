/*
 * buttons.h
 *
 * Модуль, обрабатывающий опрос состояния кнопок с подавлением дребезга, обеспечивающий обнаружение повторов при долгом нажатии 
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */


#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <avr/io.h>

// Внимание! Если меняете порты и пини, не забудьте в main.h поправить маски неиспользуемых пинов. Иначе возможны значительные токи утечки в режиме энергосбережения
// Note! If changing ports and pin numbers, do not forget to change the unused pin masks in the main.h. Otherwise current leaks in power-save mode are possible

// Буква порта и пин, к которому подключена левая кнопка
#define BUTTON_A_PORT_LETTER D
#define BUTTON_A_PIN_NUM 3

// Буква порта и пин, к которому подключена правая кнопка
#define BUTTON_B_PORT_LETTER D
#define BUTTON_B_PIN_NUM 4

#define BUTTONS_BOUNCE_PRESSED_TIMEOUT MICROSECONDS_TO_MICROTIMER(5000) // Количество отсчётов microtimer (см. pulling.h) в течение которых подавляется дребезг при нажатии
#define BUTTONS_BOUNCE_RELEASED_TIMEOUT MICROSECONDS_TO_MICROTIMER(2000) // Количество отсчётов microtimer (см. pulling.h) в течение которых подавляется дребезг при отпускании

#define BUTTONS_LONG_TICKS 61   // Количество тиков в течении которых комбинация кнопок должна удерживатьяс, прежде чем появится событие BUTTON_EVENT_LONG 
#define BUTTONS_FIRST_REPEAT_WAIT_TICKS 15 // Пазуа в тиках после BUTTON_EVENT_LONG перед первым BUTTON_EVENT_REPEAT, при удержании кнопки
#define BUTTONS_REPEAT_TIME_SHRINK_FORMULA(prevtime) ((prevtime) - ((prevtime + 3) >> 2)) // Формула уменьшения времени ожидания перед повторами
#define BUTTONS_REPEATS_TO_SPEED_UP_FORMULA(repeattime) (8 - (repeattime >> 1)) // Формула вычисления количества повторов прежде чем время перед повторами уменьшится

#define BUTTON_KIND_MASK 0xF0   // Маска бит в результате функции buttons_pull(), кодирующих комбинацию нажатых кнопок
#define BUTTON_KIND_A 0x10      // Левая кнопка
#define BUTTON_KIND_B 0x20      // Права кнопка
#define BUTTON_KIND_BOTH (BUTTON_KIND_A | BUTTON_KIND_B) // Обе кнопки вместе

#define BUTTON_EVENT_MASK 0x0F  // Маска бит в результате функции buttons_pull(), кодирующих событие, относящееся к нажатым кнопкам
#define BUTTON_EVENT_NONE 0     // Кнопки не нажаты
#define BUTTON_EVENT_PRESSED 1  // Обнаружено нажатие кнопки или комбинации кнопок
#define BUTTON_EVENT_RELEASED 2 // Обнаружено отпускание ранее нажатой комбинации кнопок
#define BUTTON_EVENT_HOLD 3     // Кнопка или комбинация в данный момент удерживается нажатой
#define BUTTON_EVENT_LONG 4     // Обнаружено долгое нажатие (оно же первый повтор)
#define BUTTON_EVENT_REPEAT 5   // Периодически появляется для уже нажатой кнопки, частота появления увеличивается с долготой удержания


/* Инициализирует подсистему опроса кнопок */
void buttons_init();

/* Вызывает pull() (см pulling.h) и опрашивает состояние кнопок 
 * Возвращает BUTTON_EVENT_NONE, либо комбинацию одного из BUTTON_KIND_ - (указывающего к какой или каким кнопкам относится событие) и  BUTTON_EVENT_ указывающего на событие */
uint8_t buttons_pull();

/* Отменяет текующую комбинацию. До отпускания нажатых кнопок и их повторного нажатия, buttons_pull будет возвращать BUTTON_EVENT_NONE */
void buttons_cancel();

/* При удержании кнопки нажатой частота появления события BUTTON_EVENT_REPEAT постепенно возрастает. После вызова этой функции частота сбросится на начальную */
void buttons_repeat_bump();

/* Периодически вызывает buttons_pull и ождает пока будет возвращено не BUTTONS_NONE и не BUTTON_EVENT_HOLD, возвращает это значение
 * Если timeout > 0 завершится раньше и вернёт 0 по истечении timeout тиков (1 тик прмерно 1/61 секунды. см. pulling.h)
 * Если exit_on_sec_change != завершится и вернёт 0, если seconds_counter (см. rtc.h) != prev_sec_cntr) */
uint8_t button_wait(uint16_t timeout, uint8_t exit_on_sec_change, uint8_t prev_sec_cntr);



#endif /* BUTTONS_H_ */