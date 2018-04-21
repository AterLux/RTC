﻿/*
 * power_manager.h
 *
 * Подсистема для контроля внешнего питания, обеспечния работы в режиме энергосбережения и рестарта после восстановления питания
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#include <avr/io.h>
#include "tools.h"

// Внимание! Если меняете порты и пини, не забудьте в main.h поправить маски неиспользуемых пинов. Иначе возможны значительные токи утечки в режиме энергосбережения
// Note! If changing ports and pin numbers, do not forget to change the unused pin masks in the main.h. Otherwise current leaks in power-save mode are possible

// Буква порта, номер пина на нём, и соответствующий ему номер регистра PCINT, на которых располагается сигнал наличия питания
#define POWER_PRESENTS_PORT_LETTER B
#define POWER_PRESENTS_PIN_NUM 0
#define POWER_PRESENTS_PCINT_NUM 0


/* Вызывается при обнаружении пропадания внешнего питания.
 * Функция должна как можно скорее завершить работу всех используемых периферийных модулей контроллера, освободить все используемые порты,
 * отключить все прерывания, которые не будут использоваться и т.д., т.е. подготовить контроллер к работе в режиме энергосбережения.
 * При запуске ПО, т.е. при сбросе или включении контроллера, и при вызове power_manager_run(), если внешнее питание отсутствует (низкий уровень на 
 * контрольном пине), то сразу, ещё до первого вызова routine_zero(), будет вызвана эта функция и совершён переход в режим энергосбережения  */
void terminate_on_power_loss();

/* Основное тело приложения, вызывается из power_manager_run() а также по окончании процесса выхода из режима энергосбережения
 * Эта процедура должно выполняться бесконечно. 
 * При входе в неё прерывания отключены, после завершения необходимых настроек, прерывания должны быть включены.
 * При пропадании питания работа процедуры будет прервана в обработчике прерывания по изменению уровня на входе, 
 * затем будет вызван terminate_on_power_loss(), затем, после восстановления питания, указатель стэка будет сброшен опять на начало и
 * опять будет вызвана routine_zero() */ 
void routine_zero();

/* Отключает прерывания, сбрасывает указатель стэка на начало, помещает в него нулевой адрес возврата и переходит на routine_zero().
 * routine_zero() не должна завершаться, но если она всё-таки завершится, то произойдёт переход на вектор сброса */
#define start_routine_zero() \
  do {  \
    asm volatile (  \
      "cli \n" \
      "ldi r16, (" VALUE_OF_MACRO(RAMEND) " >> 8)\n" \
      "out __SP_H__, r16 \n" \
      "ldi r16, (" VALUE_OF_MACRO(RAMEND) " & 0xFF)\n" \
      "out __SP_L__, r16 \n" \
      "clr r1 \n" \
      "push r1 \n" \
      "push r1 \n" \
      "jmp routine_zero \n" \
    ); \
  } while (0)


/* Запускает монитор питания и вызывает routine_zero(). Возврат из этой функции никогда не просходит.
 * Следует учесть, что если внешние питание отсутствует (низкий уровень на контрольном пине), то сначала будет вызвана terminate_on_power_loss(),
 * и лишь после восстановления питания - routine_zero() */
void power_manager_run();


#endif /* POWER_MANAGER_H_ */