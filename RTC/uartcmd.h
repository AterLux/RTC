/*
 * uartcmd.h
 *
 * Обработчик асинхронно принимаемых команд по UART, также обеспечивает синхронное формирование исходящих ответов
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef UARTCMD_H_
#define UARTCMD_H_


#define UARTCMD_PREFIX 'C'
#define UARTCMD_MAX_SIZE 16

#define UARTCMD_REPLY_PREFIX 'R'

#ifdef F_CPU
  #define UART_F_CPU F_CPU
#else
  #error "F_CPU is not defined. Set F_CPU in Project properties -> Toolchain -> Symbols"
#endif  
//#define UART_F_CPU 4000000UL

#if defined(F_CPU) && (F_CPU != UART_F_CPU)
  #error "UART_F_CPU different from F_CPU! Update UART_F_CPU in uartcmd.h"
#endif

#define UART_BAUDRATE 38400UL



#include <avr/io.h>
#include <avr/pgmspace.h>



/* Настраивает USART и запускает прерывание */
void uart_init();

/* Останавливаем USART и прерывание */
void uart_shutdown();


/* Возвращает размер очередной полученной команды (в байтах). 0 если команда не получена */
uint8_t uart_has_command();

/* Возвращает следующий байт из принятой команды */
uint8_t uart_next();

/* Заканчивает приём текущей команды и разрешает приём следующей */
void uart_skip();


/* Отправляет байт */
uint8_t uart_send(uint8_t d);

/* Отправляет очередной байт ответа. После отправки последнего байта отправляет байт контрольной суммы */
uint8_t uart_reply(uint8_t d);

/* Отсылает в ответе указанное число байт из энергонезависимой памяти. После отправки последнего байта отправляет байт контрольной суммы */
uint8_t uart_reply_pgm(PGM_VOID_P p_pgm, uint8_t size);

/* Подготваливает к отправке ответа с кодом code и указанным числом дополнительных байт. Если paramlen == 0, отправляет байт контрольной суммы */
uint8_t uart_start_reply(uint8_t code, uint8_t paramlen);






#endif /* UARTCMD_H_ */