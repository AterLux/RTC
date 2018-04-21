/*
 * pulling.h
 *
 * Подсистема производящая периодические действия, в том числе управление яркостью дисплея, обработку команд UART и т.п.
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef PULLING_H_
#define PULLING_H_

#include <avr/io.h>


#define BRIGHTNESS_PORT_LETTER D
#define BRIGHTNESS_PIN_NUM 7

#define BRIGHTNESS_MIN_VALUE 1
#define BRIGHTNESS_MAX_VALUE 20

#define DEFAULT_BRIGHTNESS_MAIN 14
#define DEFAULT_BRIGHTNESS_ALT 10

#define UART_COMMAND_BOOTLOADER 'B'  // "LST"

#define UART_COMMAND_VERSION 'V'
#define UART_REPLY_VERSION 'v' // 10: "RTC", maj, min, build_lo, build_hi, year % 100, month, day

#define UART_COMMAND_GET_TIME 'T' 
#define UART_REPLY_GET_TIME 't' // 13: year, month, day, hour, minute, second, 1/256 sec, (last set time)ls_year, ls_month, ls_day, ls_hour, ls_minute, ls_second

#define UART_COMMAND_SET_TIME 'S' // year, month, day, hour, minute, second, 1/256 sec
#define UART_REPLY_SET_TIME 's' 

#define UART_COMMAND_READ_CALIBRATION 'C' 
#define UART_REPLY_READ_CALIBRATION 'c' // 2: cal_lo, cal_hi (signed)

#define UART_COMMAND_WRITE_CALIBRATION 'W' // 2: cal_lo, cal_hi (signed)
#define UART_REPLY_WRITE_CALIBRATION 'w'

#define UART_REPLY_ERROR 'e'
#define UART_ERROR_WRONG_COMMAND 1
#define UART_ERROR_PARAMS_NOT_ENOUGH 2
#define UART_ERROR_WRONG_PARAMS 3

// Адрес в EEPROM, знаковое число два байта, младшим вперёд, с битовой инверсией. Число прибавляемое к прочитанному значению температуры (в 1/16 градусах цельсия)
#define EEADDR_TEMPERATURE_CALIBRATION 8

// Адреса в EEPROM. Индексы основной и альтернативной яркости, действительтное значение яркости вычисляется по таблице
#define EEADDR_BRIGHTNESS_MAIN 10
#define EEADDR_BRIGHTNESS_ALT 11

#define TEMPERATURE_25DEGREE_ADC_VALUE_X64 (348 * 64)
#define TEMPERATURE_ADC_TO_DEGREE_KOEF 68157L

#define MICROSECONDS_TO_MICROTIMER(x) (x * (F_CPU / 1000000UL))
extern uint8_t tick_counter;

typedef struct {
  uint16_t time_remains;
  uint16_t last_tcnt;
  uint8_t last_tick;
} MicroTimer;

/* Подготавливает механизм периодических действий, запускает таймер 1 */
void pulling_prepare();

/* Останавливает таймер */
void pulling_shutdown();

/* Очищает метку времени последнего обновления часов через UART */
void clear_rtc_update_timestamp();

/* Выполняет необходимые периодические действия, при переполнении таймера 1 (61 раз в секунду) увеличивает tick_counter, возвращает значение tick_counter */
uint8_t pull();

/* Выдерживает указанную паузу, периодически вызывая pull() */
void pause_ms(uint16_t ms);

/* Замеряет сколько отсчётов таймера1 соответствуют одному отсчёту таймера2.
 * Т.к. таймер 1 работает без прескалера, а таймера 2 делает 1 отсчёт за 1/256 секунды, то результатом будет 1/256 частоты работы ЦПУ */
uint16_t measure_tick();

/* Подбирает значение OSCCAL таким образом, чтобы результат measure_tick() наиболее соответствовал заданному значению 
 * Обновляет OSCCAL и возвращает замеренное соответствующее ему значение */
uint16_t calibrate_freq(uint16_t target_tick);

/* Заполняет структуру позволяющую определить истечение указанного (в отсчётах таймера 1) времени */
void micro_timer_init(MicroTimer * p_utimer, uint16_t cycles);

/* Обновляет структуру, возвращает 1 - если заданное время вышло, 0 - если ещё нет */
uint8_t micro_timer_elapsed(MicroTimer * p_utimer);

/* Получает измеренное и скомпенсированное значение температуры, в 1/16 градусах Цельсия */
int16_t temperature_get();

/* Возврващает текущее значение калибровки температуры, в 1/16 градусах Цельсия  */
int16_t temperature_get_offset();

/* Устанавливает новое значение калибровки температуры, в 1/16 градусах Цельсия  */
void temperature_set_offset(int16_t offset);


/* Переопределяет текущее значение яркости дисплея. Управление яркостью отключается. 
 * Значение яркости задаётся в пределах BRIGHTNESS_MIN_VALUE...BRIGHTNESS_MAX_VALUE, действительное значение, передающееся на дисплей, вычисляется по таблице */
void brightness_override(uint8_t brightness);

/* Восстанавливает управление яркостью, если initial_set != 0, то устанавливает текущую яркость на дисплее */
void brightness_resume(uint8_t initial_set);

/* Устанавливает новое значение основного уровня яркости: BRIGHTNESS_MIN_VALUEюююBRIGHTNESS_MAX_VALUE */
void brightness_set_main(uint8_t brightness);

/* Возвращает значение основного уровня яркости */
uint8_t brightness_get_main();

/* Устанавливает новое значение альтернативного уровня яркости: BRIGHTNESS_MIN_VALUE...BRIGHTNESS_MAX_VALUE */
void brightness_set_alt(uint8_t brightness);

/* Возвращает значение альтернативного уровня яркости */
uint8_t brightness_get_alt();


#endif /* PULLING_H_ */