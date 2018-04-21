/*
 * power_manager.c
 *
 * Подсистема для контроля внешнего питания, обеспечния работы в режиме энергосбережения и рестарта после восстановления питания
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#include "power_manager.h"
#include <avr/interrupt.h>
#include <avr/sleep.h>

volatile static uint8_t is_power_down;

void perform_power_down();

ISR(CONCAT3(PCINT, POWER_PRESENTS_PCINT_NUM, _vect)) {
  // Если мы в режиме отключения питания, то это прерывание используется только чтобы разбудить контроллер
  if (is_power_down) return;
  
  // Если обнаружена пропажа питания
  if (PIN_READ(POWER_PRESENTS_PORT_LETTER, POWER_PRESENTS_PIN_NUM) == 0) {
    is_power_down = 1;
    // То переходим в функцию perform_power_down.
    // Из perform_power_down возврата нет (по её завершении стэк переинициализируется и будет сделан вызов rountine_zero())
    // Поэтому, чтобы компилятор не сохранял кучу регистров в этом обработчике прерывании, просто по-тихому прыгнем на perform_power_down через ассемблер 
    asm volatile (
      "rjmp perform_power_down"
    );
  }  
}

// Переходит в режим энергосбережения, где работает только таймер
void perform_power_down() {
  cli();
  is_power_down = 1;
  
  terminate_on_power_loss();
    
    
  PRR = (1 << PRTWI) | (1 << PRTIM0) | (1 << PRTIM1) | (1 << PRSPI) | (1 << PRUSART0) | (1 << PRADC);
    

  // Установим прескалер 1:8 по двум причинам:
  // 1) чтобы прерывание таймера 2 отрабатывало за минимум 1 цикл осциллятора, иначе есть риск повторного пробуждения контроллера. 
  //    При частоте 1 МГц, 1 такт осциллятора таймера занимает около 31 такта процессора
  // 2) чтобы меньше циклов процессора проводить в ожидании обновления асинхронных регистров таймера 2, если таковые были сделаны в прерывании
  CLKPR = (1 << CLKPCE);
  CLKPR = (1 << CLKPS1) | (1 << CLKPS0); // 1:8
    
  set_sleep_mode(SLEEP_MODE_PWR_SAVE); // Режим сна - POWER DOWN
  sleep_enable(); // Разрешение перехода в режим сна
    
  while(EECR & (1<<EEPE));  // Ждём завершения операций EEPROM, на всякий случай
    
  for(;;) {
    // Ожидаем  обновления асинхронных регистров таймера 2
    while (ASSR & ((1 << TCR2AUB) | (1 << TCR2BUB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCN2UB))) { };
    cli(); // отключение прерываний
    if (PIN_READ(POWER_PRESENTS_PORT_LETTER, POWER_PRESENTS_PIN_NUM) != 0) { // Если показалось, что питание появилось...
      uint8_t c = 255;
      while ((c != 0) && (PIN_READ(POWER_PRESENTS_PORT_LETTER, POWER_PRESENTS_PIN_NUM) != 0)) { // Предпримем подавление дребезга
        c--;
      }      
      if (c == 0) break;
    }     
    // Отключение BOD перед входом в режим сна
//    sleep_bod_disable(); /* Правильнее использовать предопределённый макрос, но у меня Naggy ругается на него и это раздражает, поэтому 3 строчки снизу делают, по сути, то же самое что и sleep_bod_disable()
    uint8_t x = (MCUCR | (1 << BODS)) & ~(1 << BODSE);
    MCUCR = x | (1 << BODSE);
    MCUCR = x;/**/
    
    sei(); // включение прерываний
    sleep_cpu(); // Вход в режим сна 
  }
    
  cli();
  is_power_down = 0;
  // Возвращаем прескалер
  CLKPR = (1 << CLKPCE);
  CLKPR = (1 << CLKPS0); // 1:2
   
  PRR = 0;
    
  start_routine_zero();
}

void power_manager_run() {
  cli();
  
  CONCAT2(PCMSK, POWER_PRESENTS_PCINT_NUM) |= (1 << POWER_PRESENTS_PIN_NUM); // Разрешение в PCMSK прерывания от пина индикации питания
  PCICR |= (1 << CONCAT2(PCIE, POWER_PRESENTS_PCINT_NUM)); // Разрешение в PCICR прервания по изменению пина
  
  if (PIN_READ(POWER_PRESENTS_PORT_LETTER, POWER_PRESENTS_PIN_NUM) == 0) { // Если индикации питания нет, то идём спать
    perform_power_down();
  }    
  is_power_down = 0;
  start_routine_zero();
}

