/*
 * RTC.c
 *
 * Проект "Часы реального времени"
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#include "main.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include "display/display.h"
#include "display/fonts.h"
#include "display/text.h"
#include "eeprom.h"
#include "rtc.h"

#include "uartcmd.h"
#include "pulling.h"
#include "power_manager.h"
#include "buttons.h"
#include "digits.h"

#include "generated/localization.h"
#include "generated/digits_font.h"

#include "build_version.h"
#include "tools.h"
#include "pix.h"

void terminate_on_power_loss() {
  displayhw_shutdown();
  uart_shutdown();
  pulling_shutdown();
    
  TCCR1A = 0;
  TCCR1B = 0;
  ACSR |= (1 << ACD); 
  ADCSRA &= ~(1 << ADEN);
  TWCR &= ~(1 << TWEN);
  UCSR0B = 0;
  SPCR = 0;

}  


/* Adjuster - это пара процедур: adjuster_start() и adjuster_pull(), которые помогают реализовать увеличение или уменьшение значения одной кнопкой
 * отрисовывают иконку-подсказку в правой части дисплея, adjuster_pull() возвращает одно из 5 значений, описанных ниже.
 * Используется в меню "Калибровка хода" "калибровка температуры", "Температурный коэффициент" */
#define ADJUSTER_NONE 0     // Ничего не происходит
#define ADJUSTER_DECREASE 1 // Необходимо уменьшить значение на 1 шаг
#define ADJUSTER_INCREASE 2 // Необходимо увеличить значение на 1 шаг
#define ADJUSTER_ZERO 3     // Нажаты обе кнопки вместе
#define ADJUSTER_RETURN 4   // Нажата левая кнопка - возрват в предыдущее меню

#define ADJUSTER_TIME_TO_SWAP 150 // Время в тиках пуллинга (1/61 секунды), когда направление меняется на противоположное, если кнопка не нажата
#define ADJUSTER_TIME_WARN 60     // Время в тиках до смены направления, когда иконка начинает быстро мигать

#define ADJUSTER_BLINK_MASK_WAIT 0x20
#define ADJUSTER_BLINK_MASK_TIMEOUT 0x04

#define _ADJUSTER_FLAG_DOWN 1
#define _ADJUSTER_FLAG_DRAWN 2
static uint8_t adjuster_flags;
static uint8_t adjuster_select_timer;
static uint8_t adjuster_prev_tick;

static PROGMEM const LocalizedString * const day_week_names[] = {&str_day_su, &str_day_mo, &str_day_tu, &str_day_we, &str_day_th, &str_day_fr, &str_day_sa};

// Рисует иконку-подсказку. pix - код иконки, at_right = 0 - слева, не ноль - спарва
void draw_hint_pix(uint8_t pix, uint8_t at_right) {
  uint8_t x = at_right ? (DISPLAY_WIDTH - PIX_WIDTH) : 0;
  if (pix == PIX_EMPTY) {
    display_fill_region(DISPLAY_PAGES - PIX_HEIGHT_PAGES, x, PIX_HEIGHT_PAGES, PIX_WIDTH, 0, 0);
  } else {
    display_sprite(DISPLAY_PAGES - PIX_HEIGHT_PAGES, x, &pix_data[pix], PIX_HEIGHT_PAGES, PIX_WIDTH);
  }
  display_end();
}

// Запускает adjuster 
void adjuster_start() {
  buttons_cancel();
  adjuster_select_timer = 0;
  adjuster_flags = _ADJUSTER_FLAG_DRAWN;
  adjuster_prev_tick = tick_counter;
  draw_hint_pix(PIX_INCREASE, 1);
}

// Отрисовывает иконку-подсказку и возвращает какое действие нужно совершить со значением
uint8_t adjuster_pull() {
  uint8_t btn = buttons_pull();
  uint8_t res = ADJUSTER_NONE;
  if (btn != 0) {
    uint8_t bev = btn & BUTTON_EVENT_MASK;
    uint8_t bkn = btn & BUTTON_KIND_MASK;
    if (bkn == BUTTON_KIND_A) {
      if (bev == BUTTON_EVENT_RELEASED) return ADJUSTER_RETURN;
    } else if (bkn == BUTTON_KIND_BOTH) {
      res = ADJUSTER_ZERO;
    } else if (bkn == BUTTON_KIND_B) {
      if ((adjuster_select_timer != 0) && ((bev == BUTTON_EVENT_PRESSED) || (bev == BUTTON_EVENT_LONG) || (bev == BUTTON_EVENT_REPEAT))) {
        res = (adjuster_flags & _ADJUSTER_FLAG_DOWN) ? ADJUSTER_DECREASE : ADJUSTER_INCREASE;
      }
      adjuster_select_timer = ADJUSTER_TIME_TO_SWAP;
    }
  }
  uint8_t tc = tick_counter;
  if (adjuster_prev_tick != tc) {
    adjuster_prev_tick = tc;
    uint8_t repaint = 0;
    if (adjuster_select_timer == 0) {
      if (((adjuster_flags & _ADJUSTER_FLAG_DRAWN) == 0) != ((tc & ADJUSTER_BLINK_MASK_WAIT) == 0)) {
        adjuster_flags ^= _ADJUSTER_FLAG_DRAWN;
        repaint = 1;
      }
    } else {
      adjuster_select_timer--;
      if (adjuster_select_timer == 0) {
        repaint = 1;
        adjuster_flags ^= _ADJUSTER_FLAG_DOWN;
      } else if (adjuster_select_timer < ADJUSTER_TIME_WARN) {
        if (((adjuster_flags & _ADJUSTER_FLAG_DRAWN) == 0) != ((tc & ADJUSTER_BLINK_MASK_TIMEOUT) == 0)) {
          adjuster_flags ^= _ADJUSTER_FLAG_DRAWN;
          repaint = 1;
        }
      } else if ((adjuster_flags & _ADJUSTER_FLAG_DRAWN) == 0) {
        adjuster_flags |= _ADJUSTER_FLAG_DRAWN;
        repaint = 1;
      }
    }    
    if (repaint) {
      draw_hint_pix((adjuster_flags & _ADJUSTER_FLAG_DRAWN) ? ((adjuster_flags & _ADJUSTER_FLAG_DOWN) ? PIX_DECREASE : PIX_INCREASE) : PIX_EMPTY, 1);
    }
  }
  return res;
}

// Вычисляет с какой начинать рисовать нечто шириной width, чтобы оно было посредине, но не левее left
static uint8_t center_x(uint8_t width, uint8_t left) {
  if (width > DISPLAY_WIDTH) return left;
  uint8_t x = (uint8_t)(DISPLAY_WIDTH - width) >> 1;
  return (x < left) ? left : x;
}

// Рисует текст по середине, очищая обалсти слева и справа, с учётом отступа pad от краёв
static void center_text(uint8_t page, const LocalizedString * p_loc, uint8_t pad) {
  PGM_P p_str = get_string(p_loc);
  text_set_pos(page, pad);
  text_pad(center_x(text_width(p_str), pad));
  text_print(p_str);
  text_pad(DISPLAY_WIDTH - pad);
}

// Рисует по середине число и следующий за ним текст, очищает обалсти слева и справа, с учётом отступа pad от краёв
static void center_num_text(uint8_t page, int16_t num, uint8_t decpoint, uint8_t with_plus, const LocalizedString * p_postfix, uint8_t pad) {
  uint8_t w = text_snum_width(num, decpoint);
  if (with_plus) {
    if (num > 0) {
      w += text_char_width('+');
    } else {
      with_plus = 0;
    }     
  }
  PGM_P p_str = p_postfix ? get_string(p_postfix) : 0;
  if (p_str) w += 4 + text_width(p_str);
  text_set_pos(page, pad);
  text_pad(center_x(w, pad));
  if (with_plus) text_char('+');
  text_snum(num, decpoint);
  if (p_str) {
    text_blank(4);
    text_print(p_str);
  }    
  text_pad(DISPLAY_WIDTH - pad);
}

// Рисует текст так, чтобы он был прижат вправо к endx но не левее startx, очищает область слева от startx до текста
static void right_text(uint8_t page, uint8_t startx, uint8_t endx, const LocalizedString * p_loc) {
  PGM_P p_str = get_string(p_loc);
  uint16_t w = text_width(p_str);
  text_set_pos(page, startx);
  if (w < (endx - startx)) {
    text_pad(endx - w);
  }
  text_print(p_str);
}

// Подменю выбор языка
static void show_menu_lang() {
  draw_hint_pix(PIX_RETURN, 0);
  draw_hint_pix(PIX_NEXT, 1);
  for(;;) {
    text_set_inversion(1);
    center_text(0, &str_menu_lang, 0);
    text_set_inversion(0);
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_text(2, &str_language_name, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    display_end();
    for(;;) {
      uint8_t btn = button_wait(0, 0, 0);
      if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        uint8_t lang = get_language();
        lang++;
        if (lang >= LANGUAGE_COUNT) lang = 0;
        set_language(lang);
        break;
      } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        eeprom_write(EEADDR_LANG, get_language());
        return;
      }
    }
  }
}

// Увеличивает разряд единиц
static uint8_t cycle_ones(uint8_t num) {
  uint8_t h = 0;
  while (num >= 10) {
    num -= 10;
    h += 10;
  }
  return (num >= 9) ? h : (h + num + 1);
}

#define _FIELD_SECONDS 0
#define _FIELD_MINUTES_L 1
#define _FIELD_MINUTES_H 2
#define _FIELD_HOURS 3

// Подменю настройка времени
static void show_menu_set_time() {
  uint8_t field = _FIELD_SECONDS;
  uint8_t blink = 0;
  uint8_t digits[8];
  uint8_t prev_digits[8];
  uint8_t repaint_all = 1;
  uint8_t blink_timer = 0;
  RtcValues rtc;
  uint8_t sec_c;
  for(;;) {
    sec_c = seconds_counter;
    rtc_latch(&rtc, 0);
    two_digits(rtc.hour, &digits[0], 0);
    digits[2] = DIGIT_TIME_SEPARATOR;
    two_digits(rtc.minute, &digits[3], 1);
    digits[5] = DIGIT_TIME_SEPARATOR;
    two_digits(rtc.second, &digits[6], 1);
    if (blink) {
      switch (field) {
        case _FIELD_SECONDS:
          digits[6] = DIGIT_EMPTY;
          digits[7] = DIGIT_EMPTY;
          break;
        case _FIELD_MINUTES_L:
          digits[4] = DIGIT_EMPTY;
          break;
        case _FIELD_MINUTES_H:
          digits[3] = DIGIT_EMPTY;
          break;
        default: // _FIELD_HOURS
          digits[0] = DIGIT_EMPTY;
          digits[1] = DIGIT_EMPTY;
          break;
      }
    }
    uint8_t x = 0;
    for (uint8_t i = 0 ; i < 8 ; i++) {
      if (repaint_all || (digits[i] != prev_digits[i])) {
        prev_digits[i] = digits[i];
        digit_output(digits[i], x);
      }
      x += DIGIT_WIDTH;
    }
    display_end();
    repaint_all = 0;
    uint8_t btn;
    do {
      uint8_t tc = tick_counter;
      btn = buttons_pull();
      if (tc != tick_counter) {
        blink_timer++;
        if (blink_timer >= 20) {
          blink_timer = 0;
          blink ^= 1;
          break;
        }
      }        
    } while ((btn == 0) && (seconds_counter == sec_c));
    if ((btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) || (btn == (BUTTON_KIND_B | BUTTON_EVENT_LONG)) || (btn == (BUTTON_KIND_B | BUTTON_EVENT_REPEAT))) {
      cli();
      rtc_latch(&rtc, 0);
      switch (field) {
        case _FIELD_SECONDS:
          rtc.second = 0;
          break;
        case _FIELD_MINUTES_L: 
          rtc.minute = cycle_ones(rtc.minute);
          break;
        case _FIELD_MINUTES_H: 
          rtc.minute += 10;
          if (rtc.minute >= 60) rtc.minute -= 60;
          break;
        default: // _FIELD_HOURS
          if (++rtc.hour >= 24) rtc.hour = 0;
          break;
      }
      rtc_update(&rtc, field == _FIELD_SECONDS, 0);
      sei();
      clear_rtc_update_timestamp();
      blink = 0;
      blink_timer = 0;
    } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
      if (field == _FIELD_HOURS) return;
      field++;
      blink = 1;
      blink_timer = 0;
    } 
    
  }
}

#define _FIELD_YEAR_H 0
#define _FIELD_YEAR_L 1
#define _FIELD_MONTH 2
#define _FIELD_DAY 3

// Подменю настройка даты
static void show_menu_set_date() {
  uint8_t field = _FIELD_YEAR_H;
  uint8_t blink = 0;
  uint8_t digits[5];
  uint8_t prev_digits[5];
  uint8_t repaint_all = 1;
  uint8_t blink_timer = 0;
  uint8_t known_d = 0, known_m = 0;
  RtcValues rtc;
  uint8_t sec_c;
  for(;;) {
    sec_c = seconds_counter;
    rtc_latch(&rtc, 0);
    if ((field ==_FIELD_YEAR_H) || (field == _FIELD_YEAR_L)) {
      digits[0] = 2;
      digits[1] = two_digits(rtc.year, &digits[2], 1);
      digits[4] = DIGIT_EMPTY;
      if (blink) {
        if (field == _FIELD_YEAR_H) {
          digits[2] = DIGIT_EMPTY;
        } else {
          digits[3] = DIGIT_EMPTY;
        }          
      }
      if (repaint_all) {
        right_text(0, DIGIT_WIDTH * 5, DISPLAY_WIDTH, &str_date_year);
        display_fill_region(2, DIGIT_WIDTH * 5, 2, DISPLAY_WIDTH - DIGIT_WIDTH * 5, 0, 0);
      }
    } else {
      two_digits(rtc.day, &digits[0], 0);
      digits[2] = DIGIT_DATE_SEPARATOR;
      two_digits(rtc.month, &digits[3], 0);
      if (blink) {
        if (field == _FIELD_MONTH) {
          digits[3] = DIGIT_EMPTY;
          digits[4] = DIGIT_EMPTY;
        } else {
          digits[0] = DIGIT_EMPTY;
          digits[1] = DIGIT_EMPTY;
        }
      }        
      if (repaint_all) right_text(0, DIGIT_WIDTH * 5, DISPLAY_WIDTH, (field == _FIELD_MONTH) ? &str_date_month : &str_date_day);
      if (repaint_all || (known_d != rtc.day) || (known_m != rtc.month)) {
        known_d = rtc.day;
        known_m = rtc.month;
        uint8_t dw = day_of_week(known_d, known_m, rtc.year);
        if (dw >= 7) dw -= 7;
        right_text(2, DIGIT_WIDTH * 5, DISPLAY_WIDTH, pgm_read_ptr(&day_week_names[dw]));
      }
    }    
    uint8_t x = 0;
    for (uint8_t i = 0 ; i < 5 ; i++) {
      if (repaint_all || (digits[i] != prev_digits[i])) {
        prev_digits[i] = digits[i];
        digit_output(digits[i], x);
      }
      x += DIGIT_WIDTH;
    }
    display_end();
    repaint_all = 0;
    uint8_t btn;
    do {
      uint8_t tc = tick_counter;
      btn = buttons_pull();
      if (tc != tick_counter) {
        blink_timer++;
        if (blink_timer >= 20) {
          blink_timer = 0;
          blink ^= 1;
          break;
        }
      }        
    } while ((btn == 0) && (seconds_counter == sec_c));
    if ((btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) || (btn == (BUTTON_KIND_B | BUTTON_EVENT_LONG)) || (btn == (BUTTON_KIND_B | BUTTON_EVENT_REPEAT))) {
      cli();
      rtc_latch(&rtc, 0);
      switch (field) {
        case _FIELD_YEAR_H:
          rtc.year += 10;
          if (rtc.year >= 100) rtc.year -= 100;
          break;
        case _FIELD_YEAR_L:
          rtc.year = cycle_ones(rtc.year);
          break;
        case _FIELD_MONTH: 
          if (++rtc.month > 12) rtc.month = 1;
          break;
        default: //_FIELD_DAY:
          rtc.day++;
          if ((rtc.day > 28) && (rtc.day > days_in_month(rtc.month, rtc.year))) rtc.day = 1;
          break;
      }
      rtc_update(&rtc, 0, 0);
      sei();
      clear_rtc_update_timestamp();
      blink = 0;
      blink_timer = 0;
    } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
      if (field == _FIELD_DAY) return;
      field++;
      repaint_all = 1;
      blink = 1;
      blink_timer = 0;
    } 
    
  }
}

// Подменю выбор шрифта текста
static void show_menu_text_font() {
  uint8_t font = eeprom_read(EEADDR_TEXT_FONT, 0);
  draw_hint_pix(PIX_RETURN, 0);
  draw_hint_pix(PIX_NEXT, 1);
  for(;;) {
    text_set_inversion(1);
    center_text(0, &str_menu_text_font, 0);
    text_set_inversion(0);
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_text(2, &str_font_example, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    display_end();
    for(;;) {
      uint8_t btn = button_wait(0, 0, 0);
      if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        font++;
        if (font >= FONTS_COUNT) font = 0;
        text_select_font(pgm_read_ptr(&fonts_list[font]));
        break;
      } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        eeprom_write(EEADDR_TEXT_FONT, font);
        return;
      }
    }
  }
}

// Подменю выбор начертания цифр
static void show_menu_digits_font() {
  display_clear();
  draw_hint_pix(PIX_RETURN, 0);
  draw_hint_pix(PIX_NEXT, 1);
  uint8_t digits[5];
  uint8_t repaint_all = 1;
  uint8_t prev_digits[5];
  for(;;) {
    uint8_t sec_c = seconds_counter;
    RtcValues rtc;
    rtc_latch(&rtc, 0);
    two_digits(rtc.minute, &digits[0], 1);
    digits[2] = DIGIT_TIME_SEPARATOR;
    two_digits(rtc.second, &digits[3], 1);
    uint8_t x = (DISPLAY_WIDTH - DIGIT_WIDTH * 5) / 2;
    for (uint8_t i = 0 ; i < 5 ; i++) {
      if (repaint_all || (prev_digits[i] != digits[i])) {
        prev_digits[i] = digits[i];
        digit_output(digits[i], x);
      }
      x += DIGIT_WIDTH;
    }
    display_end();
    repaint_all = 0;
    for(;;) {
      uint8_t btn = button_wait(0, 1, sec_c);
      if (!btn) {
        break;
      } else if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        uint8_t digf = digits_current_font() + 1;
        if (digf >= DIGITS_FONTS_COUNT) digf = 0;
        digits_select_font(digf);
        repaint_all = 1;
        break;
      } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        eeprom_write(EEADDR_DIGITS_FONT, digits_current_font());
        return;
      } 
    }
  }
}

// Подменю выбора анимационного эффекта для цифр
static void show_menu_fx() {
  display_clear();
  draw_hint_pix(PIX_RETURN, 0);
  draw_hint_pix(PIX_NEXT, 1);
  uint8_t digits[5];
  uint8_t repaint_all = 1;
  uint8_t prev_digits[5];
  uint8_t animating = 0;
  uint8_t animation_phase = 0;
  uint8_t sec_c = seconds_counter;
  uint8_t prev_sec = sec_c - 1;
  for(;;) {
    RtcValues rtc;
    if (sec_c != prev_sec) {
      rtc_latch(&rtc, 0);
      two_digits(rtc.minute, &digits[0], 1);
      digits[2] = DIGIT_TIME_SEPARATOR;
      two_digits(rtc.second, &digits[3], 1);
      prev_sec = sec_c;
      if (digits_get_animation() != DIGITS_ANIMATION_NONE) {
        animating = 1;
      }
    }    
    uint8_t x = (DISPLAY_WIDTH - DIGIT_WIDTH * 5) / 2;
    if (!animating || repaint_all || (animation_phase >= 32)) {
      animating = 0;
      for (uint8_t i = 0 ; i < 5 ; i++) {
        if (repaint_all || (prev_digits[i] != digits[i])) {
          prev_digits[i] = digits[i];
          digit_output(digits[i], x);
        }
        x += DIGIT_WIDTH;
      }
    } else {
      for (uint8_t i = 0 ; i < 5 ; i++) {
        if (prev_digits[i] != digits[i]) {
          digit_transition(prev_digits[i], digits[i], x, animation_phase);
        }
        x += DIGIT_WIDTH;
      }
    }    
    display_end();
    repaint_all = 0;
    uint8_t new_phase;
    do {
      uint8_t btn = buttons_pull();
      if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        uint8_t digf = digits_get_animation() + 1;
        if (digf >= DIGITS_ANIMATIONS_COUNT) digf = 0;
        digits_set_animation(digf);
        repaint_all = 1;
      } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        eeprom_write(EEADDR_ANIMATION, digits_get_animation());
        return;
      } 
      sec_c = seconds_counter;
      new_phase = animating ? (rtc_get_subtime() >> 2) : 0;
    } while ((sec_c == prev_sec) && !repaint_all && (!animating || (new_phase == animation_phase)));
    animation_phase = new_phase;
  }
}


// Приглашение "Сохранить?" 
static uint8_t prompt_save() {
  buttons_cancel();
  text_set_inversion(1);
  center_text(0, &str_prompt_save, 0);
  text_set_inversion(0);
  draw_hint_pix(PIX_CANCEL, 0);
  draw_hint_pix(PIX_OK, 1);
  for(;;) {
    uint8_t btn = buttons_pull();
    if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
      return 0;
    } else if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
      return 1;
    }      
  }  
}

// Подменю настройки уровней яркости
static void show_menu_brightness(uint8_t is_alt) {
  uint8_t val = is_alt ? brightness_get_alt() : brightness_get_main();
  int16_t first_val = val;
  brightness_override(val);
  uint8_t eq_prev = 0;
  adjuster_start();
  text_set_inversion(1);
  center_text(0, is_alt ? &str_menu_brightness_alt : &str_menu_brightness_main, 0);
  text_set_inversion(0);
  for(;;) {
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_num_text(2, val, 0, 0, 0, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    uint8_t eq = val == first_val;
    if (eq != eq_prev) {
      eq_prev = eq;
      draw_hint_pix(eq ? PIX_RETURN : PIX_SAVE, 0);
    }
    display_end();
    
    uint8_t adj;
    do {
      adj = adjuster_pull();
    } while (adj == ADJUSTER_NONE);
        
    if (adj == ADJUSTER_RETURN) {
      if ((val != first_val) && prompt_save()) {
        if (is_alt) {
          brightness_set_alt(val);
        } else {
          brightness_set_main(val);
        }
      }
      brightness_resume(0);
      return;
    } else if (adj == ADJUSTER_DECREASE) {
      if (val > BRIGHTNESS_MIN_VALUE) {
        val--;
        brightness_override(val);
      }        
    } else if (adj == ADJUSTER_INCREASE) {
      if (val < BRIGHTNESS_MAX_VALUE) {
        val++;
        brightness_override(val);
      }
    } else if (adj == ADJUSTER_ZERO) {
      uint8_t nv = is_alt ? DEFAULT_BRIGHTNESS_ALT : DEFAULT_BRIGHTNESS_MAIN;
      if (nv != val) {
        val = nv;
        brightness_override(val);
      }
    }
  }    
}


// Подменю калибровки хода часов
static void show_menu_calibrate() {
  int16_t calibr = rtc_get_calibration();
  int16_t first_calibr = calibr;
  uint8_t eq_prev = 0;
  adjuster_start();
  for(;;) {
    int16_t secperday = ((int32_t)calibr * 3375 + 32768) >> 16;
    center_num_text(0, secperday, 1, 1, &str_calib_spd, 1);

    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    int16_t ppm = ((int32_t)calibr * 3906  + 32768) >> 16;
    center_num_text(2, ppm, 0, 1, &str_calib_ppm, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    uint8_t eq = calibr == first_calibr;
    if (eq != eq_prev) {
      eq_prev = eq;
      draw_hint_pix(eq ? PIX_RETURN : PIX_SAVE, 0);
    }
    
    uint8_t adj;
    do {
      adj = adjuster_pull();
    } while (adj == ADJUSTER_NONE);
        
    if (adj == ADJUSTER_RETURN) {
      if ((calibr != first_calibr) && prompt_save()) {
        rtc_set_calibration(calibr);
        clear_rtc_update_timestamp();
      }
      return;
    } else if (adj == ADJUSTER_DECREASE) {
      if (calibr >= -32752) calibr = (calibr - 16) & 0xFFF0;
      if (calibr == 0) buttons_repeat_bump();
    } else if (adj == ADJUSTER_INCREASE) {
      if (calibr < 32752) calibr = (calibr + 16) & 0xFFF0;
      if (calibr == 0) buttons_repeat_bump();
    } else if (adj == ADJUSTER_ZERO) {
      calibr = 0;
    }
  }  
}

// Подменю калибровки температуры
static void show_menu_temp() {
  int16_t init_offset = temperature_get_offset();
  int16_t offset = init_offset;
  int16_t temp = temperature_get();
  uint8_t eq_prev = 0;
  adjuster_start();
  text_set_inversion(1);
  center_text(0, &str_menu_temp, 0);
  text_set_inversion(0);
  for(;;) {
    int16_t temp_deg = ((temp - init_offset + offset) * 10 + 8) >> 4;
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_num_text(2, temp_deg, 1, 0, &str_temp_degree, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    uint8_t eq = offset == init_offset;
    if (eq != eq_prev) {
      eq_prev = eq;
      draw_hint_pix(eq ? PIX_RETURN : PIX_SAVE, 0);
    }
    display_end();
    
    int16_t prev_temp = temp;
    uint8_t adj;
    do {
      adj = adjuster_pull();
      temp = temperature_get();
    } while ((adj == ADJUSTER_NONE) && (temp == prev_temp));
        
    if (adj == ADJUSTER_RETURN) {
      if ((offset != init_offset) && prompt_save()) {
        temperature_set_offset(offset);
      }
      return;
    } else if (adj == ADJUSTER_DECREASE) {
      if (offset >= -796) offset = (offset - 4) & 0xFFFC;
      if (offset == 0) buttons_repeat_bump();
    } else if (adj == ADJUSTER_INCREASE) {
      if (offset <= 796) offset = (offset + 4) & 0xFFFC;
      if (offset == 0) buttons_repeat_bump();
    } else if (adj == ADJUSTER_ZERO) {
      offset = 0;
    }
  }  
}

// Подменю калибровки температурного коэффициента
static void show_menu_temp_koef() {
  uint8_t init_koef = rtc_get_thermal_koef();
  uint8_t koef = init_koef;
  uint8_t eq_prev = 0;
  adjuster_start();
  text_set_inversion(1);
  center_text(0, &str_menu_temp_koef, 0);
  text_set_inversion(0);
  for(;;) {
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_num_text(2, koef, 3, 0, &str_temp_koef_ppmpc, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    uint8_t eq = koef == init_koef;
    if (eq != eq_prev) {
      eq_prev = eq;
      draw_hint_pix(eq ? PIX_RETURN : PIX_SAVE, 0);
    }
    display_end();
    
    uint8_t adj;
    do {
      adj = adjuster_pull();
    } while (adj == ADJUSTER_NONE);
        
    if (adj == ADJUSTER_RETURN) {
      if ((koef != init_koef) && prompt_save()) {
        rtc_set_thermal_koef(koef);
      }
      return;
    } else if (adj == ADJUSTER_DECREASE) {
      if (koef > 0) koef--;
    } else if (adj == ADJUSTER_INCREASE) {
      if (koef < 254) koef++;
    } else if (adj == ADJUSTER_ZERO) {
      koef = DEFAULT_THERMAL_KOEF_MIL;
    }
  }  
}


// Подменю автоматизированной калибровки скорости ЦП
static void show_menu_fcpu() {
  draw_hint_pix(PIX_RETURN, 0);
  draw_hint_pix(PIX_CALIBRATE, 1);
  text_set_inversion(1);
  center_text(0, &str_menu_fcpu, 0);
  text_set_inversion(0);
  for(;;) {
    uint8_t sec_c = seconds_counter;
    uint16_t cpu_kHz = (measure_tick() * 16777UL + 32768) >> 16;
    text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
    center_num_text(2, cpu_kHz, 0, 0, &str_fcpu_khz, PIX_WIDTH);
    text_set_limit_x(DISPLAY_WIDTH);
    display_end();
    for(;;) {
      uint8_t btn = button_wait(0, 1, sec_c);
      if (!btn) {
        break;
      } else if (btn == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        uint8_t prev_osccal = OSCCAL;
        text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
        center_text(2, &str_fcpu_calibration, PIX_WIDTH);
        text_set_limit_x(DISPLAY_WIDTH);
        display_end();
        uint16_t calibr = calibrate_freq(TARGET_TICK_PERIOD);
        // Если откалибровали значение и оно в пределах +/- 2%, то сохраним его
        if ((calibr >= (TARGET_TICK_PERIOD - TARGET_TICK_PERIOD / 50)) && (calibr <= (TARGET_TICK_PERIOD + TARGET_TICK_PERIOD / 50))) {
          uint8_t new_osccal = OSCCAL;
          if (new_osccal != prev_osccal) {
            eeprom_write(EEADDR_OSCAL, new_osccal);
          }
        }         
        break;
      } else if (btn == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        return;
      }
    }
  }
}


#define _MENU_LANG 0
#define _MENU_SET_TIME 1
#define _MENU_SET_DATE 2
#define _MENU_TEXT_FONT 3
#define _MENU_DIGIT_FONT 4
#define _MENU_FX 5
#define _MENU_BRIGHTNESS_MAIN 6
#define _MENU_BRIGHTNESS_ALT 7
#define _MENU_CALIBRATE 8
#define _MENU_TEMP_CAL 9
#define _MENU_TEMP_KOEF 10
#define _MENU_F_CPU 11
#define _MENU_INFO 12

// Главное меню
static void show_menu() {
  buttons_cancel();
  uint8_t menu_item = 0;
  uint8_t info_page = 0;
  for(;;) {
    if (menu_item == _MENU_INFO) {
      text_set_limit_x(DISPLAY_WIDTH);
      draw_hint_pix(PIX_RETURN, 0);
      draw_hint_pix(PIX_NEXT, 1);
      
      if (info_page == 1) { 
        // Вторая страница информации: информация об авторе
        center_text(0, &str_info_author, 0);
        text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
        center_text(2, &str_info_author2, PIX_WIDTH);
        text_set_limit_x(DISPLAY_WIDTH);
      } else {
        // Первая страница информации: версия прошивки и дата сборки
        PGM_P p_str = get_string(&str_info_firmware);
        uint8_t tx = text_width(p_str) + 4;
        uint8_t wl = text_unum_width(VERSION_MAJOR, 0) + text_unum_width(VERSION_MINOR, 0) + text_unum_width(BUILD_NUMBER, 0) + text_char_width('.') * 2;
        uint8_t x1;
        uint8_t x2;
        if ((tx + wl) > DISPLAY_WIDTH) {
          x1 = 0;
          x2 = DISPLAY_WIDTH - wl;
        } else {
          x1 = (uint8_t)(DISPLAY_WIDTH - (tx + wl)) >> 1;
          x2 = x1 + tx;
        }
        text_set_pos(0, 0);
        text_pad(x1);
        text_print(p_str);
        text_pad(x2);
        text_set_pos(0, x2);
        text_unum(VERSION_MAJOR, 0);
        text_char('.');
        text_unum(VERSION_MINOR, 0);
        text_char('.');
        text_unum(BUILD_NUMBER, 0);
        text_pad(DISPLAY_WIDTH);
        
        p_str = get_string(&str_info_firmware_date);
        tx = text_width(p_str) + 4;
        wl = text_unum_width(BUILD_DAY, 0) + text_unum_width(BUILD_MONTH, 0) + text_unum_width(BUILD_YEAR, 0) + text_char_width('.') * 2;
        if ((tx + wl) > (DISPLAY_WIDTH - PIX_WIDTH * 2)) {
          x1 = PIX_WIDTH;
          x2 = (DISPLAY_WIDTH - PIX_WIDTH) - wl;
        } else {
          x1 = (uint8_t)(DISPLAY_WIDTH - (tx + wl)) >> 1;
          x2 = x1 + tx;
        }
        text_set_limit_x(DISPLAY_WIDTH - PIX_WIDTH);
        text_set_pos(2, PIX_WIDTH);
        text_pad(x1);
        text_print(p_str);
        text_pad(x2);
        text_set_pos(2, x2);
        text_unum(BUILD_DAY, 0);
        text_char('.');
        text_unum(BUILD_MONTH, 0);
        text_char('.');
        text_unum(BUILD_YEAR, 0);
        text_pad(DISPLAY_WIDTH - PIX_WIDTH);
        
        text_set_limit_x(DISPLAY_WIDTH);
      }
    } else {
      const LocalizedString * p_loc;
      switch (menu_item) {
        case _MENU_LANG             : p_loc = &str_menu_lang;             break;
        case _MENU_SET_TIME         : p_loc = &str_menu_set_time;         break;
        case _MENU_SET_DATE         : p_loc = &str_menu_set_date;         break;
        case _MENU_TEXT_FONT        : p_loc = &str_menu_text_font;        break;
        case _MENU_DIGIT_FONT       : p_loc = &str_menu_digits_font;      break;
        case _MENU_FX               : p_loc = &str_menu_fx;               break;
        case _MENU_BRIGHTNESS_MAIN  : p_loc = &str_menu_brightness_main;  break;
        case _MENU_BRIGHTNESS_ALT   : p_loc = &str_menu_brightness_alt;   break;
        case _MENU_CALIBRATE        : p_loc = &str_menu_calibrate;        break;
        case _MENU_TEMP_CAL         : p_loc = &str_menu_temp;             break;
        case _MENU_TEMP_KOEF        : p_loc = &str_menu_temp_koef;        break;
        case _MENU_F_CPU            : p_loc = &str_menu_fcpu;             break;
        default: p_loc = 0;
      }
      draw_hint_pix(PIX_NEXT, 0);
      draw_hint_pix(PIX_ENTER, 1);
      center_text(0, p_loc, 0);
      display_fill_region(2, PIX_WIDTH, 2, DISPLAY_WIDTH - PIX_WIDTH * 2, 0, 0);
    }
    display_end();
    for(;;) {
      uint8_t pb = button_wait(3600, 0, 0);
      if (pb == 0) {
        return;
      } else if (pb == (BUTTON_KIND_A | BUTTON_EVENT_LONG)) {
        return;
      } else if (pb == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) {
        if (menu_item == _MENU_INFO) return;
        menu_item++;
        break;
      } else if (pb == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED)) {
        switch (menu_item) {
          case _MENU_INFO:
            info_page++;
            if (info_page > 1) info_page = 0;
            break;
          case _MENU_LANG             : show_menu_lang();         break;
          case _MENU_SET_TIME         : show_menu_set_time();     break;
          case _MENU_SET_DATE         : show_menu_set_date();     break;
          case _MENU_TEXT_FONT        : show_menu_text_font();    break;
          case _MENU_DIGIT_FONT       : show_menu_digits_font();  break;
          case _MENU_FX               : show_menu_fx();           break;
          case _MENU_BRIGHTNESS_MAIN  : show_menu_brightness(0);  break;
          case _MENU_BRIGHTNESS_ALT   : show_menu_brightness(1);  break;
          case _MENU_CALIBRATE        : show_menu_calibrate();    break;
          case _MENU_TEMP_CAL         : show_menu_temp();         break;
          case _MENU_TEMP_KOEF        : show_menu_temp_koef();    break;
          case _MENU_F_CPU            : show_menu_fcpu();         break;
        }
        buttons_cancel();
        break;
      }      
    } 
  }
}

static void show_date() {
  uint8_t known_d, known_m, known_y;
  RtcValues rtc;
  uint8_t prevsec = seconds_counter;
  rtc_latch(&rtc, 0);
  uint8_t digits[5];
  
  uint16_t timer = 0;
  uint8_t prevtick = tick_counter;
  for(;;) {
    known_d = rtc.day;
    known_m = rtc.month;
    known_y = rtc.year;
    two_digits(known_d, &digits[0], 0);
    digits[2] = DIGIT_DATE_SEPARATOR;
    two_digits(known_m, &digits[3], 0);
    for (uint8_t i = 0 ; i < 5 ; i++) {
      digit_output(digits[i], i * DIGIT_WIDTH);
    }
    uint16_t year = 2000 + known_y;
    uint8_t x = DISPLAY_WIDTH - text_unum_width(year, 0);
    if (x < 5 * DIGIT_WIDTH) x = 5 * DIGIT_WIDTH;
    text_set_pos(0, 5 * DIGIT_WIDTH);
    text_pad(x);
    text_unum(year, 0);
    
    uint8_t dw = day_of_week(known_d, known_m, known_y);
    if (dw >= 7) dw -= 7;
    LocalizedString * p_ls = pgm_read_ptr(&day_week_names[dw]);
    right_text(2, 5 * DIGIT_WIDTH, DISPLAY_WIDTH, p_ls);
    display_end();
    
    for(;;) {
      uint8_t pb = buttons_pull();
      if ((pb == (BUTTON_KIND_A | BUTTON_EVENT_PRESSED)) || (pb == (BUTTON_KIND_B | BUTTON_EVENT_PRESSED))) {
        return;
      }
      if ((pb & BUTTON_KIND_MASK) == BUTTON_KIND_B) {
        timer = 0;
      }
      if (tick_counter != prevtick) {
        prevtick = tick_counter;
        timer++;
        if (timer >= 610) return;
      }
      uint8_t s = seconds_counter;
      if (s != prevsec) {
        prevsec = s;
        rtc_latch(&rtc, 0);
        if ((known_d != rtc.day) || (known_m != rtc.month) || (known_y != rtc.year)) break;
      }
    } 
  }
  
}

void routine_zero() {
  sei();
  
  pulling_prepare();
  
  buttons_init();
  uart_init();
  
  while (!display_init()) {
    pause_ms(50);
  }
  
  brightness_resume(1);
  
  text_reset();
  
  uint8_t digits[8];
  uint8_t prevdigits[8];
  for (uint8_t i = 0 ; i < 8; i++) {
    digits[i] = 0;
    prevdigits[i] = 0xFE;
  }
    
  uint8_t prevsec = seconds_counter - 1;
  uint8_t animating = 0;
  uint8_t animation_step = 0;
  uint8_t repaint_all = 1;
  
  for(;;) {
    uint8_t s;
    uint8_t new_anim_step;
    do {
      uint8_t b = buttons_pull();
      if (b == (BUTTON_EVENT_PRESSED | BUTTON_KIND_B)) {
        show_date();
        repaint_all = 1;
      } else if (b == (BUTTON_EVENT_PRESSED | BUTTON_KIND_A)) {
        show_menu();
        repaint_all = 1;
      }
      s = seconds_counter;
      new_anim_step = animating ? (rtc_get_subtime() >> 2) : 0;
    } while ((s == prevsec) && !repaint_all && (!animating || (new_anim_step == animation_step)));
    if (repaint_all) { 
      animating = 0;
    } else if ((prevsec != s) && (digits_get_animation() != DIGITS_ANIMATION_NONE)) {
      animating = 1;
    }
    if (animating) {
      animation_step = new_anim_step;
      if (animation_step >= 32) {
        animation_step = 0;
        animating = 0;
      }       
    }
    if ((prevsec != s) || repaint_all) {
      RtcValues rtc;
      rtc_latch(&rtc, 0);
    
      two_digits(rtc.hour, &digits[0], 0);
      digits[2] = DIGIT_TIME_SEPARATOR;
      two_digits(rtc.minute, &digits[3], 1);
      digits[5] = DIGIT_TIME_SEPARATOR;
      two_digits(rtc.second, &digits[6], 1);
    }
    prevsec = s;
    

    uint8_t slx = 0;
    if (animating) {
      for (uint8_t i = 0 ; i < 8 ; i++) {
        if (slx >= DISPLAY_WIDTH) break;
        if (prevdigits[i] != digits[i]) {
          digit_transition(prevdigits[i], digits[i], slx, animation_step);
        }        
        slx += DIGIT_WIDTH;
      }
    } else {    
      for (uint8_t i = 0 ; i < 8 ; i++) {
        if (slx >= DISPLAY_WIDTH) break;
        if (repaint_all || (prevdigits[i] != digits[i])) {
          digit_output(digits[i], slx);
          prevdigits[i] = digits[i];
        }        
        slx += DIGIT_WIDTH;
      }
    }    
    repaint_all = 0;
    display_end();
  }
}

int main(void)
{
  cli();
  CLKPR = (1 << CLKPCE);
  CLKPR = (1 << CLKPS0); // 1:2
  
  ACSR |= (1 << ACD);

  DDRB = 0;
  DDRC = 0;
  DDRD = 0;
  PORTB = UNUSED_PINS_MASK_B;
  PORTC = UNUSED_PINS_MASK_C;
  PORTD = UNUSED_PINS_MASK_D; 
  
  if (rtc_is_running()) {
    rtc_wait();
    union {
      uint8_t b[4];
      uint32_t dw;
    } t;
    t.b[0] = GPIOR1;
    t.b[1] = GPIOR2;
    t.b[2] = OCR2A;
    t.b[3] = OCR2B;
// структура выше эквивалентна ((uint32_t)OCR2B << 24U) | ((uint32_t)OCR2A << 16U) | (GPIOR2 << 8U) | GPIOR1, но компилируется в меньший код
    if (!rtc_restore(t.dw)) {
      rtc_init();
    }
  } else {
    rtc_init();
  }

  uint8_t osccal = eeprom_read(EEADDR_OSCAL, 0xFF);
  if (osccal != 0xFF) OSCCAL = osccal;
  uint8_t t;
  
  t = eeprom_read(EEADDR_LANG, LANGUAGE_DEFAULT);
  set_language((t < LANGUAGE_COUNT) ? t : 0);
  
  t = eeprom_read(EEADDR_DIGITS_FONT, 0);
  digits_select_font((t < DIGITS_FONTS_COUNT) ? t : 0);
  
  t = eeprom_read(EEADDR_TEXT_FONT, 0);
  text_select_font((t < FONTS_COUNT) ? pgm_read_ptr(&fonts_list[t]) : 0);
  
  t = eeprom_read(EEADDR_ANIMATION, DIGITS_ANIMATION_NONE);
  digits_set_animation((t < DIGITS_ANIMATIONS_COUNT) ? t : 0);
  
  // Запуск power_manager_run приводит к вызову routine_zero() и никогда не возвращается.
  power_manager_run(); 
}

