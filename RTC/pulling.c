/*
 * pulling.c
 *
 * Подсистема производящая периодические действия, в том числе управление яркостью дисплея, обработку команд UART и т.п.
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "pulling.h"
#include "uartcmd.h"
#include "display/display.h"
#include "main.h"
#include "rtc.h"
#include "tools.h"
#include "eeprom.h"
#include "build_version.h"

uint8_t tick_counter;
static RtcValues last_rtc_update_time;
static int16_t temperature_offset;
static uint16_t temp_adc_acc;
static uint8_t temp_adc_acc_count;
static uint32_t temp_big_acc;
static uint8_t temp_big_acc_count;
static int16_t temperature_measured = (25 * 16);

static uint8_t brightness_overriden;
static uint8_t brightness_current;
static uint8_t brightness_main;
static uint8_t brightness_alt;


static PROGMEM uint8_t const brightness_map[] = {0, 2, 5, 10, 16, 23, 31, 40, 51, 64, 77, 92, 108, 125, 144, 163, 184, 207, 231, 255};

uint8_t brightness_val_to_abs(uint8_t val) {
  uint8_t idx = (val < BRIGHTNESS_MIN_VALUE) ? 0 : val - BRIGHTNESS_MIN_VALUE;
  if (idx >= array_size(brightness_map)) idx = array_size(brightness_map) - 1;
  return pgm_read_byte(&brightness_map[idx]);
}



void pulling_prepare() {
  TCCR1A = 0;
  TCCR1B = (1 << CS10);
  
  temperature_offset = ~eeprom_read_int16(EEADDR_TEMPERATURE_CALIBRATION, 0xFFFF);
  
  brightness_main = brightness_val_to_abs(eeprom_read(EEADDR_BRIGHTNESS_MAIN, DEFAULT_BRIGHTNESS_MAIN));
  brightness_alt = brightness_val_to_abs(eeprom_read(EEADDR_BRIGHTNESS_ALT, DEFAULT_BRIGHTNESS_ALT));
  brightness_current = 0;
  brightness_overriden = 1;
  DDR_SET_INPUT(BRIGHTNESS_PORT_LETTER, BRIGHTNESS_PIN_NUM);
  PORT_SET_HIGH(BRIGHTNESS_PORT_LETTER, BRIGHTNESS_PIN_NUM);
  
  ADMUX = (1 << REFS1) | (1 << REFS0) | (1 << MUX3);
  temp_adc_acc = 0;
  temp_adc_acc_count = 0;
  temp_big_acc_count = 0;
  temp_big_acc = 0;
  ADCSRB = 0;
  ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS1) | (1 << ADPS2);
}

void pulling_shutdown() {
  PORT_SET_LOW(BRIGHTNESS_PORT_LETTER, BRIGHTNESS_PIN_NUM); // Выключени подтяжки для экономии энергии
  ADCSRA = 0;
  TCCR1B = 0;
}


void on_tick() {
  if (temp_adc_acc_count >= 64) {
    temp_big_acc += temp_adc_acc;
    temp_adc_acc = 0;
    temp_adc_acc_count = 0;
    ADCSRA |= (1 << ADSC);
    
    if (++temp_big_acc_count >= 64) {
      uint16_t adc_avg64 = ((temp_big_acc + 32U) >> 6U);
      int16_t adc_rel_25c = adc_avg64 - TEMPERATURE_25DEGREE_ADC_VALUE_X64;
      int16_t temp_rel_25c = ((int32_t)adc_rel_25c  * TEMPERATURE_ADC_TO_DEGREE_KOEF + 32768) >> 16;
      int16_t temperature = ((temp_rel_25c + 2) >> 2) + (25 * 16);
      temperature_measured = temperature + temperature_offset;
      rtc_update_temperature(temperature_measured);
      
      temp_big_acc_count = 0;
      temp_big_acc = 0;
    }
  }
  if (!brightness_overriden) {
    uint8_t target_brightness = PIN_READ(BRIGHTNESS_PORT_LETTER, BRIGHTNESS_PIN_NUM) ? brightness_main : brightness_alt;
    if (brightness_current != target_brightness) {
      uint8_t stp = (brightness_current >> 4) + 1;
      if (target_brightness > brightness_current) {
        brightness_current = ((target_brightness - brightness_current) > stp) ? (brightness_current + stp) : target_brightness;
      } else {
        brightness_current = ((brightness_current - target_brightness) > stp) ? (brightness_current - stp) : target_brightness;
      }
      display_set_contrast(brightness_current);
    }
  }
}

void run_bootloader() {
  union {
    uint8_t b[4];
    uint32_t dw;
  } t;
  t.dw = rtc_detach();
  OCR2B = t.b[3];
  OCR2A = t.b[2];
  GPIOR2 = t.b[1];
  GPIOR1 = t.b[0];
  rtc_wait();
  jump_to_bootloader();
}

static PROGMEM uint8_t const reply_version[] = {'R', 'T', 'C', VERSION_MAJOR, VERSION_MINOR, (BUILD_NUMBER & 255), (BUILD_NUMBER >> 8), (BUILD_YEAR - 2000), BUILD_MONTH, BUILD_DAY};
  
void uart_reply_error(uint8_t code) {
  uart_start_reply(UART_REPLY_ERROR, 1);
  uart_reply(code);
}

void on_uart_command(uint8_t l) {
  switch (uart_next()) {
    case UART_COMMAND_BOOTLOADER: 
      if ((l == 4) && (uart_next() == 'L') && (uart_next() == 'S') && (uart_next() == 'T')) {
        run_bootloader();
      } else {
        uart_reply_error(UART_ERROR_WRONG_COMMAND);
      }
      break;
    case UART_COMMAND_VERSION:
      uart_start_reply(UART_REPLY_VERSION, 10);
      uart_reply_pgm(&reply_version, 10);
      break; 
    case UART_COMMAND_GET_TIME: {
      RtcValues rtc;
      uint8_t sub_second = rtc_latch(&rtc, 1);
      uart_start_reply(UART_REPLY_GET_TIME, 13);
      uart_reply(rtc.year);
      uart_reply(rtc.month);
      uart_reply(rtc.day);
      uart_reply(rtc.hour);
      uart_reply(rtc.minute);
      uart_reply(rtc.second);
      uart_reply(sub_second);
      
      uart_reply(last_rtc_update_time.year);
      uart_reply(last_rtc_update_time.month);
      uart_reply(last_rtc_update_time.day);
      uart_reply(last_rtc_update_time.hour);
      uart_reply(last_rtc_update_time.minute);
      uart_reply(last_rtc_update_time.second);
      
      break;
    }
    case UART_COMMAND_SET_TIME: {
      if (l < 8) {
        uart_reply_error(UART_ERROR_PARAMS_NOT_ENOUGH);
        break;
      }        
      RtcValues rtc;
      rtc.year   = uart_next();
      rtc.month  = uart_next();
      rtc.day    = uart_next();
      rtc.hour   = uart_next();
      rtc.minute = uart_next();
      rtc.second = uart_next();
      uint8_t sub_second = uart_next();
      if ((rtc.second >= 60) || (rtc.minute >= 60) || (rtc.hour >= 24) ||
          (rtc.year >= 100) || (rtc.month < 1) || (rtc.month > 12) || 
          (rtc.day < 1) || (rtc.day > days_in_month(rtc.month, rtc.year))) {
        uart_reply_error(UART_ERROR_WRONG_PARAMS);
        break;
      }
      rtc_update(&rtc, 1, sub_second);
      last_rtc_update_time = rtc;
      uart_start_reply(UART_REPLY_SET_TIME, 0);
      break;
    }
    case UART_COMMAND_READ_CALIBRATION: {
      int16_t cal = rtc_get_calibration();
      uart_start_reply(UART_REPLY_READ_CALIBRATION, 2);
      uart_reply(cal);
      uart_reply(cal >> 8);
      break;
    }
    case UART_COMMAND_WRITE_CALIBRATION: {
      if (l < 2) {
        uart_reply_error(UART_ERROR_PARAMS_NOT_ENOUGH);
      } else {
        int16_t cal = uart_next() | (uart_next() << 8);
        rtc_set_calibration(cal);
        uart_start_reply(UART_REPLY_WRITE_CALIBRATION, 0);
      }      
      break;
    }
    default: 
      uart_reply_error(UART_ERROR_WRONG_COMMAND);
  }
}

void clear_rtc_update_timestamp() {
  last_rtc_update_time.second = 0;
  last_rtc_update_time.minute = 0;
  last_rtc_update_time.hour   = 0;
  last_rtc_update_time.day    = 0;
  last_rtc_update_time.month  = 0;
  last_rtc_update_time.year   = 0;
}

uint8_t pull() {
  if (temp_adc_acc_count < 64) {
    if ((ADCSRA & (1 << ADSC)) == 0) {
      temp_adc_acc += ADC;
      if (++temp_adc_acc_count < 64) {
        ADCSRA |= (1 << ADSC);
      }
    }
  }
  if (TIFR1 & (1 << TOV1)) {
    TIFR1 |= (1 << TOV1);
    tick_counter++;
    on_tick();
  }
  uint8_t cmdl = uart_has_command();
  if (cmdl > 0) {
    on_uart_command(cmdl);
    uart_skip();
  }
  return tick_counter;
}

void pause_ms(uint16_t ms) {
  uint16_t start_tcnt = TCNT1;
  uint8_t start_tick = tick_counter;
  if ((TIFR1 & (1 << TOV1)) && (start_tcnt < 32768)) start_tick++;
  
  uint32_t wait_cycles = ms * (F_CPU / 1000UL);
  uint16_t wait_ticks = (uint16_t)(wait_cycles >> 16);
  uint16_t end_tcnt = start_tcnt + (uint16_t)wait_cycles;
  if (end_tcnt < start_tcnt) wait_ticks++;
  uint8_t prev_tick = tick_counter;
  do {
    uint8_t tc = pull();
    uint8_t d = tc - prev_tick;
    if (d > wait_ticks) return;
    wait_ticks -= d;
    prev_tick = tc;
  } while (wait_ticks > 0);
  do {
    pull();
  } while ((TCNT1 < end_tcnt) && ((TIFR1 & (1 << TOV1)) == 0) && (tick_counter == prev_tick));
}

uint16_t measure_tick() {
  while (ASSR & ((1 << TCR2AUB) | (1 << TCR2BUB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCN2UB))) {}
  uint8_t prevtcnt;
  do {
    prevtcnt = TCNT2;
  } while ((prevtcnt == 0) || (prevtcnt == 255));
  uint8_t tcnt;
  uint8_t sreg = SREG;
  cli();
  do {
    tcnt = TCNT2;
  } while (tcnt == prevtcnt);
  uint16_t stmr = TCNT1;
  uint16_t protect = 8192;
  while (TCNT2 == tcnt) {
    if (--protect == 0) return 0;
  }
  stmr = TCNT1 - stmr;
  SREG = sreg;
  return stmr;
}

uint16_t calibrate_freq(uint16_t target_tick) {
  uint16_t tick = measure_tick();
  if (tick == 0) return 0;
  uint8_t bestv = OSCCAL;
  uint16_t besttick = tick;
  uint16_t bestdif = (tick > target_tick) ? (tick - target_tick) : (target_tick - tick);
  uint8_t mask = bestv & 0x80;
  uint8_t min = 0;
  uint16_t max = 0x80;
  do {
    uint8_t v = (min + max) >> 1;
    OSCCAL = v | mask;
    tick = measure_tick();
    if (tick == 0) break;
    uint16_t dif;
    if (tick > target_tick) {
      dif = tick - target_tick;
      max = v;
    } else {
      dif = target_tick - tick;
      min = v + 1;
    }
    if (dif < bestdif) {
      bestdif = dif;
      besttick = tick;
      bestv = v;
      if (dif == 0) break;
    }
  } while (min < max);
  OSCCAL = bestv | mask;
  return besttick;
}

void micro_timer_init(MicroTimer * p_utimer, uint16_t cycles) {
  p_utimer->time_remains = cycles;
  uint16_t tcnt = TCNT1;
  uint8_t tc = tick_counter;
  if (TIFR1 & (1 << TOV1)) tc++;
  p_utimer->last_tcnt = tcnt;
  p_utimer->last_tick = tc;
}

uint8_t micro_timer_elapsed(MicroTimer * p_utimer) {
  uint16_t rem = p_utimer->time_remains;
  if (rem == 0) return 1;
  uint8_t ltc = p_utimer->last_tick;
  uint16_t ltcnt = p_utimer->last_tcnt;
  uint16_t tcnt = TCNT1;
  uint8_t tc = tick_counter;
  if (TIFR1 & (1 << TOV1)) tc++;
  if (tc != ltc) {
    if ((tc != (uint8_t)(ltc + 1)) || (tcnt >= ltcnt)) {
      p_utimer->time_remains = 0;
      return 1;
    }
    p_utimer->last_tick = tc;    
  }
  uint16_t d = tcnt - ltcnt;
  if (d >= rem) {
    p_utimer->time_remains = 0;
    return 1;
  }
  p_utimer->time_remains = rem - d;
  return 0;
}

int16_t temperature_get() {
  return temperature_measured;//temperature_measured;
}

int16_t temperature_get_offset() {
  return temperature_offset;
}

void temperature_set_offset(int16_t offset) {
  if (temperature_offset != offset) {
    temperature_measured += offset - temperature_offset;
    temperature_offset = offset;
    eeprom_write_int16(EEADDR_TEMPERATURE_CALIBRATION, ~offset);
    rtc_update_temperature(temperature_measured);
  }
}

void brightness_override(uint8_t brightness) {
  brightness_overriden = 1;
  brightness_current = brightness_val_to_abs(brightness);
  display_set_contrast(brightness_current);
}

void brightness_resume(uint8_t initial_set) {
  brightness_overriden = 0;
  if (initial_set) display_set_contrast(brightness_current);
}

uint8_t brightness_get_main() {
  return eeprom_read(EEADDR_BRIGHTNESS_MAIN, DEFAULT_BRIGHTNESS_MAIN);
}

void brightness_set_main(uint8_t brightness) {
  if ((brightness >= BRIGHTNESS_MIN_VALUE) && (brightness <= BRIGHTNESS_MAX_VALUE)) {
    uint8_t abs_val = brightness_val_to_abs(brightness);
    if (brightness_main != abs_val) {
      brightness_main = abs_val;
      eeprom_write(EEADDR_BRIGHTNESS_MAIN, brightness);
    }      
  }  
}

uint8_t brightness_get_alt() {
  return eeprom_read(EEADDR_BRIGHTNESS_ALT, DEFAULT_BRIGHTNESS_ALT);
}

void brightness_set_alt(uint8_t brightness) {
  if ((brightness >= BRIGHTNESS_MIN_VALUE) && (brightness <= BRIGHTNESS_MAX_VALUE)) {
    uint8_t abs_val = brightness_val_to_abs(brightness);
    if (brightness_alt != abs_val) {
      brightness_alt = abs_val;
      eeprom_write(EEADDR_BRIGHTNESS_ALT, brightness);
    }      
  }  
}

