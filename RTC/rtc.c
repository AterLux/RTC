/*
 * rtc.c
 *
 * Подсистема учёта реального времени
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include "rtc.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "build_version.h"
#include "eeprom.h"

volatile uint8_t seconds_counter;

static volatile uint8_t rtc_second = 0;
static volatile uint8_t rtc_minute = 0;
static volatile uint8_t rtc_hour = 0;
static volatile uint8_t rtc_day = BUILD_DAY;
static volatile uint8_t rtc_month = BUILD_MONTH;
static volatile uint8_t rtc_year = (BUILD_YEAR % 100);

/* Единица в калибровочных величинах соответствует коррекции один раз в 65536 секунд на величину 1/256 секунды, ≈ 0,06 ppm, или 5,15 мс за сутки.
 * Предел коррекции: -32768...+32767, ≈ ±1953 ppm / ±168,75 сек/сутки */
 
static int16_t rtc_calibration = 0; // Заданная настройка калибровки
static int16_t rtc_thermal_compensation = 0; // Текущая температурная компенсация
static volatile int16_t rtc_total_compensation = 0; // Итоговое значение компенсации хода, прибавляется к компенсационному счётчику.
static volatile int16_t rtc_calib_counter = 0; // Компенсационный счётчик: при его переполнении происходит прибавление или убавление 1/256 секунды.

static uint8_t rtc_thermal_koef = DEFAULT_THERMAL_KOEF_MIL; // Коэффициент температурной коррекции (ppm/°C²)/1000, т.е. значение 45 = 0,045 ppm/°C²
static int16_t rtc_known_temperature = 25 * 16; // Температура в 1/16 °C

ISR(TIMER2_COMPA_vect) {
  TIFR2 |= (1 << OCF2B);
  int16_t rcc = rtc_calib_counter;
  uint8_t prevcb = (rcc >> 8);
  int16_t cal = rtc_total_compensation;
  rcc += cal;
  rtc_calib_counter = rcc;
  uint8_t newcb = (rcc >> 8);
  if ((((prevcb ^ newcb) & 0x80) != 0) && ((((uint8_t)(cal >> 8) ^ newcb) & 0x80) != 0)) { // Счётчик перекинулся.
    int8_t add = (cal >= 0) ? 1 : -1;
    TCNT2 += add;
  }
  seconds_counter++;
  uint8_t t = rtc_second + 1;
  if (t >= 60) {
    t = rtc_minute + 1;
    if (t >= 60) {
      t = rtc_hour + 1;
      if (t >= 24) {
        t = rtc_day + 1;
        if (t > 28) {
          uint8_t days_in_month;
          // Для того, чтобы не сохранять тонны регистров и ускорить прерывание, количество дней в месяце рассчитаем тут, а не через вызов функции
          if (t == 2) {
            days_in_month = ((rtc_year & 3) == 0) ? 29 : 28; // 2100 не високосный, но кого это волнует?
          } else {
            days_in_month = 30 + (((t > 7) ? t + 1 : t) & 1);
          }
          if (t > days_in_month) {
            t = rtc_month + 1;
            if (t > 12) {
              t = rtc_year + 1;
              if (t > 99) t = 0;
              rtc_year = t;
              t = 1;
            }
            rtc_month = t;
            t = 1;
          }
        }
        rtc_day = t;
        t = 0;
      }
      rtc_hour = t;
      t = 0;
    }
    rtc_minute = t;
    t = 0;
  } 
  rtc_second = t;
}

uint32_t rtc_to_uint32(const RtcValues * p_rtc_values) {
  uint16_t add_days = p_rtc_values->day - 1;
  for (uint8_t m = 1 ; ((m < 12) && (m < p_rtc_values->month)) ; m++) {
    if (m == 2) {
      add_days += ((p_rtc_values->year & 3) == 0) ? 29 : 28;
    } else {
      add_days += 30 + (((m >= 8) ? (m + 1) : m) & 1);
    }
  }
  add_days += ((p_rtc_values->year + 3) >> 2); // Количество прошедших високосных лет
  
  return
      p_rtc_values->year * (365UL * SECONDS_PER_DAY) +
      add_days * (uint32_t)SECONDS_PER_DAY +
      p_rtc_values->hour * (uint32_t)SECONDS_PER_HOUR +
      (p_rtc_values->minute * SECONDS_PER_MINUTE +
      p_rtc_values->second);
}

void uint32_to_rtc(uint32_t value, RtcValues * p_rtc) {
  uint8_t y = 0;
  while (value >= (SECONDS_PER_NONLEAP_YEAR * 3 + SECONDS_PER_LEAP_YEAR)) {
    value -= (SECONDS_PER_NONLEAP_YEAR * 3 + SECONDS_PER_LEAP_YEAR);
    y += 4;
  }
  if (value >= SECONDS_PER_LEAP_YEAR) {
    value -= SECONDS_PER_LEAP_YEAR;
    y++;
  }
  while (value >= SECONDS_PER_NONLEAP_YEAR) {
    value -= SECONDS_PER_NONLEAP_YEAR;
    y++;
  }
  p_rtc->year = y;
  
  uint8_t m = 1;
  for(;;) {
    uint32_t sub;
    if (m == 2) {
      if ((y & 3) == 0) {
        sub = (SECONDS_PER_DAY * 29);
      } else {
        sub = (SECONDS_PER_DAY * 28);
      }
    } else if ((((m > 7) ? (m + 1) : m) & 1) != 0) {
      sub = (SECONDS_PER_DAY * 31);
    } else {
      sub = (SECONDS_PER_DAY * 30);
    }
    if (value < sub) break;
    value -= sub;
    m++;
  }
  p_rtc->month = m;
  
  uint8_t t = 1;
  while (value >= SECONDS_PER_DAY * 5) {
    value -= SECONDS_PER_DAY * 5;
    t += 5;
  }
  while (value >= SECONDS_PER_DAY) {
    value -= SECONDS_PER_DAY;
    t++;
  }
  p_rtc->day = t;
  
  t = 0;
  
  while (value >= SECONDS_PER_HOUR * 4) {
    value -= SECONDS_PER_HOUR * 4;
    t += 4;
  }
  
  while (value >= SECONDS_PER_HOUR) {
    value -= SECONDS_PER_HOUR;
    t++;
  }
  p_rtc->hour = t;
  
  uint16_t val16 = (uint16_t)value;
  t = 0;
  while (val16 >= SECONDS_PER_MINUTE * 8) {
    val16 -= SECONDS_PER_MINUTE * 8;
    t += 8;
  }
  while (val16 >= SECONDS_PER_MINUTE) {
    val16 -= SECONDS_PER_MINUTE;
    t++;
  }
  p_rtc->minute = t;
  p_rtc->second = val16;
}

uint8_t rtc_latch(RtcValues * p_rtc, uint8_t with_sub_time) {
  while (ASSR & ((1 << TCR2AUB) | (1 << TCR2BUB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCN2UB))) {  };
  uint8_t sreg = SREG;
  cli();
  p_rtc->second = rtc_second;
  p_rtc->minute = rtc_minute;
  p_rtc->hour   = rtc_hour;
  p_rtc->day    = rtc_day;
  p_rtc->month  = rtc_month;
  p_rtc->year   = rtc_year;
  if (!with_sub_time) { // Если суб-время не нужно, просто выходим
    SREG = sreg;
    return 0;
  }
  
  // Повторное ожидание освобождения регистров, на случай если сработало прерывание
  while (ASSR & (1 << TCN2UB)) {};
  uint8_t cnt = TCNT2;
  uint8_t flg = TIFR2;
  SREG = sreg;
  
  // Проверяем, что, пока мы латчили время, не наступила следующая секунда, если так - ставим счётчик долей секунды на максимальное
  if (flg & (1 << OCF2A)) return 255; 
  
  // TCNT прочитанный как 255 может означать как конец секунды, так и то что время было откалибровано назад.
  // На этот случай OCR2B установлен на значение 254, т.е. при наступлении 255 устанавливается флаг OCF2B
  // В прерывании таймера этот флаг сбрасывается. Поэтому если мы видим 255, но не видим флаг - значит была калибровка, сейчас начало секунды
  if ((cnt == 255) && ((flg & (1 << OCF2B)) == 0)) return 0;

  // Иначе просто возвращаем счётчик, это и будут доли секунды.
  return cnt;
}

uint8_t rtc_get_subtime() {
  while (ASSR & (1 << TCN2UB)) {};
  uint8_t sreg = SREG;
  cli();
  // Повторное ожидание освобождения регистров, на случай если сработало прерывание
  while (ASSR & (1 << TCN2UB)) {};
  uint8_t cnt = TCNT2;
  uint8_t flg = TIFR2;
  SREG = sreg;
  
  // Проверяем, что, пока мы латчили время, не наступила следующая секунда, если так - ставим счётчик долей секунды на максимальное
  if (flg & (1 << OCF2A)) return 255; 
  
  // TCNT прочитанный как 255 может означать как конец секунды, так и то что время было откалибровано назад.
  // На этот случай OCR2B установлен на значение 254, т.е. при наступлении 255 устанавливается флаг OCF2B
  // В прерывании таймера этот флаг сбрасывается. Поэтому если мы видим 255, но не видим флаг - значит была калибровка, сейчас начало секунды
  if ((cnt == 255) && ((flg & (1 << OCF2B)) == 0)) return 0;

  // Иначе просто возвращаем счётчик, это и будут доли секунды.
  return cnt;
}

void rtc_update(const RtcValues * p_new_time, uint8_t with_sub_time, uint8_t sub_time) {
  while (ASSR & ((1 << TCR2AUB) | (1 << TCR2BUB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCN2UB))) {  };
  uint8_t sreg = SREG;
  cli();
  if ((rtc_second  != p_new_time->second) ||
      (rtc_minute  != p_new_time->minute) ||
      (rtc_hour    != p_new_time->hour)   ||
      (rtc_day     != p_new_time->day)    ||
      (rtc_month   != p_new_time->month)  ||
      (rtc_year    != p_new_time->year)) {
    rtc_second  = p_new_time->second;
    rtc_minute  = p_new_time->minute;
    rtc_hour    = p_new_time->hour;
    rtc_day     = p_new_time->day;
    rtc_month   = p_new_time->month;
    rtc_year    = p_new_time->year;
    seconds_counter++;
  }  
  if (with_sub_time) {
    while (ASSR & (1 << TCN2UB)) {  };
    TCNT2 = (sub_time == 255) ? 254 : sub_time; // Не устанавливаем на 255, чтобы не пропустить прерывание
    TIFR2 |= (1 << OCF2A); // Сбрасываем флаг прерывания
  }    
  SREG = sreg;
}

uint8_t rtc_is_running() {
  return (ASSR & (1 << AS2));
}

static void rtc_recalc_compensation() {
  int16_t comp = rtc_calibration;
  int16_t therm_comp = rtc_thermal_compensation;
  int16_t res = comp + therm_comp;
  if (comp < 0) {
    if ((therm_comp < 0) && (res >= 0)) res = -32768; // Вообще, therm_comp всегда не меньше нуля, но на всякий случай...
  } else {
    if ((therm_comp >= 0) && (res < 0)) res = 32767;
  }
  uint8_t sreg = SREG;
  cli();
  rtc_total_compensation = res;
  SREG = sreg;
}

static void rtc_recalc_thermal_compansation() {
  int16_t dev = rtc_known_temperature - RTC_TURNOVER_TEMPERATURE_16;
  uint32_t dev_squared = (int32_t)dev * dev;  // °C² x256

  if (dev_squared > 4000000) dev_squared = 4000000; // -100...+150 °C 
  
  uint32_t ppm_125 = (dev_squared * rtc_thermal_koef + 1024) >> 11U; // ppm x125       
  
  if (ppm_125 > 244139) ppm_125 = 244139; // max 1953,1 ppm
  
  int16_t comp = ((ppm_125 * 8796U + 32768U) >> 16U);
  if (comp != rtc_thermal_compensation) {
    rtc_thermal_compensation = comp;
    rtc_recalc_compensation();
  }
}


void rtc_wait() {
  while (ASSR & ((1 << TCR2AUB) | (1 << TCR2BUB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCN2UB))) {};
}

uint32_t rtc_detach() {
  uint8_t sreg = SREG;
  cli();
  TIMSK2 = 0; // Запрещаем прерывания.
  RtcValues rtc;
  rtc_latch(&rtc, 0);
  uint8_t flg = TIFR2;
  if (!(flg & (1 << OCF2A))) TIFR2 |= (1 << TOV2); // Если перрывание таймера не на очереди, то сбросим флаг переполнения
  flg = TIFR2;
  SREG = sreg;
  uint32_t result = rtc_to_uint32(&rtc);
  // Может быть ситуация что пока мы сбрасывали флаг переполнения, наступила очередь прерывания 
  if ((flg & ((1 << OCF2A) | (1 << TOV2))) == (1 << OCF2A)) {  // тогда просто увеличим результат на единичку
    result++;
  }
  return result;
}


static void rtc_load_calibration() {
  rtc_calibration = ~eeprom_read_int16(EEADDR_RTC_CALIBRATION, 0xFFFF);
  rtc_thermal_koef = eeprom_read(EEADDR_RTC_THERMAL_KOEF, DEFAULT_THERMAL_KOEF_MIL);
  
  rtc_recalc_compensation();
}

uint8_t rtc_restore(uint32_t time) {
  rtc_wait();
  if (!(ASSR & (1 << AS2)) || (TCCR2B != ((1 << CS22) | (1 << CS20)))) return 0;
  RtcValues rtc;
  uint8_t sreg = SREG;
  cli();
  rtc_load_calibration();
  rtc_wait();
  OCR2A = 255;
  OCR2B = 254;
  rtc_wait();
  uint8_t flg = TIFR2;
  TIFR2 = (1 << TOV2) | (1 << OCF2A); // Сбрасываем флаги прерываний
  if (flg & (1 << TOV2)) { // Если, меж тем,  на очереди было прерывание, то просто увеличим показания часов
    time++;
  }
  uint32_to_rtc(time, &rtc);
  rtc_second  = rtc.second;
  rtc_minute  = rtc.minute;
  rtc_hour    = rtc.hour;
  rtc_day     = rtc.day;
  rtc_month   = rtc.month;
  rtc_year    = rtc.year;
  TIMSK2 = (1 << OCIE2A);
  SREG = sreg;
  return 1;
}

void rtc_init() {
  TIMSK2 = 0;
  ASSR = (1 << AS2);
  
  TCNT2 = 0;
  OCR2A = 255;
  OCR2B = 254;
  TCCR2A = 0;
  TCCR2B = (1 << CS22) | (1 << CS20);
  
  rtc_load_calibration();
  
  rtc_wait();
  
  
  TIFR2 = (1 << TOV2) | (1 << OCR2A) | (1 << OCR2B);
  
  TIMSK2 = (1 << OCIE2A);
}

int16_t rtc_get_calibration() {
  return rtc_calibration;
}

void rtc_set_calibration(int16_t calibration) {
  if (calibration != rtc_calibration) {
    rtc_calibration = calibration;
    eeprom_write_int16(EEADDR_RTC_CALIBRATION, ~rtc_calibration);
    rtc_recalc_compensation();
  }
}


void rtc_update_temperature(int16_t temperature16) {
  if (rtc_known_temperature != temperature16) {
    rtc_known_temperature = temperature16;
    rtc_recalc_thermal_compansation();
  }
}

void rtc_set_thermal_koef(uint8_t milppm_per_c2) {
  if (milppm_per_c2 != rtc_thermal_koef) {
    rtc_thermal_koef = milppm_per_c2;
    eeprom_write(EEADDR_RTC_THERMAL_KOEF, rtc_thermal_koef);
    rtc_recalc_thermal_compansation();
  }
}

uint8_t rtc_get_thermal_koef() {
  return rtc_thermal_koef;
}

int16_t rtc_get_total_compensation() {
  return rtc_total_compensation;
}

int16_t rtc_get_thermal_compenstaion() {
  return rtc_thermal_compensation;  
}

uint8_t days_in_month(uint8_t month, uint8_t year) {
  if ((month < 1) || (month > 12)) return 0;
  if (month == 2) return ((year & 3) == 0) ? 29 : 28;
  return 30 + (((month > 7) ? month + 1 : month) & 1);
}

// 1 - понедельник, 7 - воскресенье, 0 - выход за пределы
uint8_t day_of_week(uint8_t d, uint8_t m, uint8_t y) {
  if ((y >= 100) || (m < 1) || (m > 12)) return 0; // Контрольки
  uint8_t w = 5 + (d & 31); // max 36
  w += y + ((y + 3) >> 2); // max 160

  static PROGMEM uint8_t const month_offset_map[] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
  w += pgm_read_byte(&month_offset_map[m - 1]); // max +6 = 166
  if (((y & 3) == 0) && (m > 2)) w++; // +1 = 167
  while (w > 7) w -= 7;
  return w;
}



