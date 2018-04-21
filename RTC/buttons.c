/*
 * buttons.c
 *
 * Модуль, обрабатывающий опрос состояния кнопок с подавлением дребезга, обеспечивающий обнаружение повторов при долгом нажатии 
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include "buttons.h"
#include "pulling.h"
#include "tools.h"
#include "rtc.h"

void buttons_init() {
  DDR_SET_INPUT(BUTTON_A_PORT_LETTER, BUTTON_A_PIN_NUM);
  PORT_SET_HIGH(BUTTON_A_PORT_LETTER, BUTTON_A_PIN_NUM);

  DDR_SET_INPUT(BUTTON_B_PORT_LETTER, BUTTON_B_PIN_NUM);
  PORT_SET_HIGH(BUTTON_B_PORT_LETTER, BUTTON_B_PIN_NUM);
}

#define BUTTON_STATE_NONE 0 
#define BUTTON_STATE_PRESSED 1
#define BUTTON_STATE_WAIT_RELEASE 2

static uint8_t button_a_state;
static MicroTimer button_a_bounce;
static uint8_t button_b_state;
static MicroTimer button_b_bounce;

static uint8_t button_prev_tick;
static uint8_t buttons_kind;
static uint8_t buttons_wait;
static uint8_t buttons_repeat_time;
static uint8_t buttons_repeats_to_increase;

uint8_t buttons_pull() {
  pull();  
  uint8_t release_detected = 0;
  if (PIN_READ(BUTTON_A_PORT_LETTER, BUTTON_A_PIN_NUM)) {
    if (button_a_state != BUTTON_STATE_NONE) {
      if (micro_timer_elapsed(&button_a_bounce)) {
        if (button_a_state == BUTTON_STATE_PRESSED) release_detected = 1;
        button_a_state = BUTTON_STATE_NONE;
        micro_timer_init(&button_a_bounce, BUTTONS_BOUNCE_PRESSED_TIMEOUT);
      }
    } else {
      micro_timer_init(&button_a_bounce, BUTTONS_BOUNCE_PRESSED_TIMEOUT);
    }
  } else {
    if (button_a_state == BUTTON_STATE_NONE) {
      if (micro_timer_elapsed(&button_a_bounce)) {
        button_a_state = BUTTON_STATE_PRESSED;
        micro_timer_init(&button_a_bounce, BUTTONS_BOUNCE_RELEASED_TIMEOUT);
      }
    } else {
      micro_timer_init(&button_a_bounce, BUTTONS_BOUNCE_RELEASED_TIMEOUT);
    }
  }
  if (PIN_READ(BUTTON_B_PORT_LETTER, BUTTON_B_PIN_NUM)) {
    if (button_b_state != BUTTON_STATE_NONE) {
      if (micro_timer_elapsed(&button_b_bounce)) {
        if (button_b_state == BUTTON_STATE_PRESSED) release_detected = 1;
        button_b_state = BUTTON_STATE_NONE;
        micro_timer_init(&button_b_bounce, BUTTONS_BOUNCE_PRESSED_TIMEOUT);
      }
    } else {
      micro_timer_init(&button_b_bounce, BUTTONS_BOUNCE_PRESSED_TIMEOUT);
    }
  } else {
    if (button_b_state == BUTTON_STATE_NONE) {
      if (micro_timer_elapsed(&button_b_bounce)) {
        button_b_state = BUTTON_STATE_PRESSED;
        micro_timer_init(&button_b_bounce, BUTTONS_BOUNCE_RELEASED_TIMEOUT);
      }
    } else {
      micro_timer_init(&button_b_bounce, BUTTONS_BOUNCE_RELEASED_TIMEOUT);
    }
  }
  if (release_detected) {
    uint8_t kind = buttons_kind;
    if ((kind & BUTTON_KIND_A) && (button_a_state == BUTTON_STATE_PRESSED)) {
      button_a_state = BUTTON_STATE_WAIT_RELEASE;
    }
    if ((kind & BUTTON_KIND_B) && (button_b_state == BUTTON_STATE_PRESSED)) {
      button_b_state = BUTTON_STATE_WAIT_RELEASE;
    }
    buttons_kind = 0;
    return kind | BUTTON_EVENT_RELEASED; 
  }
  uint8_t new_kind = 0;
  if (button_a_state == BUTTON_STATE_PRESSED) new_kind |= BUTTON_KIND_A;
  if (button_b_state == BUTTON_STATE_PRESSED) new_kind |= BUTTON_KIND_B;
  if (new_kind == 0) {
    buttons_kind = 0;
    return BUTTON_EVENT_NONE;
  }    
  if (new_kind != buttons_kind) {
    buttons_kind = new_kind;
    buttons_repeat_time = 255;
    buttons_wait = BUTTONS_LONG_TICKS;
    return new_kind | BUTTON_EVENT_PRESSED;
  }
  if (button_prev_tick == tick_counter) return new_kind | BUTTON_EVENT_HOLD;
  button_prev_tick = tick_counter;
  if (buttons_wait > 0) {
    buttons_wait--;
    return new_kind | BUTTON_EVENT_HOLD;
  }
  if (buttons_repeat_time == 255) {
    buttons_repeat_time = BUTTONS_FIRST_REPEAT_WAIT_TICKS;
    buttons_wait = BUTTONS_FIRST_REPEAT_WAIT_TICKS;
    buttons_repeats_to_increase = BUTTONS_REPEATS_TO_SPEED_UP_FORMULA(BUTTONS_FIRST_REPEAT_WAIT_TICKS);
    return new_kind | BUTTON_EVENT_LONG;
  }
  if (buttons_repeats_to_increase > 0) {
    buttons_repeats_to_increase--;
  } else {
    buttons_repeat_time = BUTTONS_REPEAT_TIME_SHRINK_FORMULA(buttons_repeat_time);
    buttons_repeats_to_increase = BUTTONS_REPEATS_TO_SPEED_UP_FORMULA(buttons_repeat_time);
  }  
  buttons_wait = buttons_repeat_time;
  return new_kind | BUTTON_EVENT_REPEAT;
}

void buttons_repeat_bump() {
  if ((buttons_kind != 0) && (buttons_repeat_time != 255)) {
    buttons_repeat_time = BUTTONS_FIRST_REPEAT_WAIT_TICKS;
    buttons_wait = BUTTONS_FIRST_REPEAT_WAIT_TICKS;
    buttons_repeats_to_increase = BUTTONS_REPEATS_TO_SPEED_UP_FORMULA(BUTTONS_FIRST_REPEAT_WAIT_TICKS);
  }
}

void buttons_cancel() {
  if (button_a_state == BUTTON_STATE_PRESSED) button_a_state = BUTTON_STATE_WAIT_RELEASE;
  if (button_b_state == BUTTON_STATE_PRESSED) button_b_state = BUTTON_STATE_WAIT_RELEASE;
}

uint8_t button_wait(uint16_t timeout, uint8_t exit_on_sec_change, uint8_t prev_sec_cntr) {
  uint8_t prev_tick = tick_counter;
  for(;;) {
    uint8_t b = buttons_pull();
    if ((b != 0) && ((b & BUTTON_EVENT_MASK) != BUTTON_EVENT_HOLD)) return b;
    if (prev_tick != tick_counter) {
      prev_tick = tick_counter;
      if (timeout > 0) {
        timeout--;
        if (!timeout) return 0;
      }
    }
    if (exit_on_sec_change && (prev_sec_cntr != seconds_counter)) return 0;
  }
}
