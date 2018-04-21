/*
 * uartcmd.c
 *
 * Обработчик асинхронно принимаемых команд по UART, также обеспечивает синхронное формирование исходящих ответов
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#include "uartcmd.h"
#include <avr/interrupt.h>

#define _STATUS_READY 0
#define _STATUS_GET_SIZE 1
#define _STATUS_RECEIVING 2
#define _STATUS_COMPLETED 3

#define UART_UBRR ((UART_F_CPU + 4 * UART_BAUDRATE) / UART_BAUDRATE / 8 - 1)

static volatile uint8_t status = _STATUS_READY;
static volatile uint8_t data_size;
static volatile uint8_t in_buf_pos;
static uint8_t control_sum;

static uint8_t reply_control_sum;
static uint8_t reply_count;

uint8_t uartcmd_in_buf[UARTCMD_MAX_SIZE];

#define _rol8(v) \
   ({ \
     uint8_t t = v; \
     asm volatile ( \
       "bst %0, 7 \n" \
       "lsl %0 \n" \
       "bld %0, 0 \n" \
       : "+r" (t) \
     ); \
     t; \
   }) 


ISR(USART_RX_vect) {
  uint8_t st = status;
  if (st == _STATUS_COMPLETED) return;
  uint8_t flags = UCSR0A;
  uint8_t data = UDR0;
  if (flags & ((1 << FE0) | (1 << DOR0) | (1 << UPE0))) {
    status = (data == UARTCMD_PREFIX) ? _STATUS_GET_SIZE : _STATUS_READY;
    return;
  }
  switch (st) {
    case _STATUS_READY:
      if (data == UARTCMD_PREFIX) status = _STATUS_GET_SIZE;
      return;
    case _STATUS_GET_SIZE:
      if ((data > 0) && (data <= UARTCMD_MAX_SIZE)) {
        data_size = data;
        in_buf_pos = 0;
        control_sum = 0xFF;
        status = _STATUS_RECEIVING;
      } else if (data != UARTCMD_PREFIX) {
        status = _STATUS_READY;
      }
      return;
    case _STATUS_RECEIVING: {
      uint8_t p = in_buf_pos;
      if (p < data_size) {
        uartcmd_in_buf[p++] = data;
        in_buf_pos = p;
        control_sum = _rol8(control_sum ^ data);
      } else if (control_sum == data) {
        in_buf_pos = 0;
        status = _STATUS_COMPLETED;
      } else {
        status = (data == UARTCMD_PREFIX) ? _STATUS_GET_SIZE : _STATUS_READY;
      }
      return;
    }    
    default:
      status = _STATUS_READY;
  }
}

void uart_init() {
  UBRR0 = UART_UBRR;
  UCSR0A = (1 << U2X0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
  status = _STATUS_READY;
  UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
}

void uart_shutdown() {
  UCSR0B = 0;
}

uint8_t uart_has_command() {
  return (status == _STATUS_COMPLETED) ? data_size : 0;
}

uint8_t uart_next() {
  uint8_t p = in_buf_pos;
  if ((status == _STATUS_COMPLETED) && (p < data_size)) {
    uint8_t d = uartcmd_in_buf[in_buf_pos];
    p++;
    if (p == data_size) status = _STATUS_READY;
    in_buf_pos = p;
    return d;
  } else {
    return 0;
  }
}

void uart_skip() {
  if (status == _STATUS_COMPLETED) status = _STATUS_READY;
}

uint8_t uart_send(uint8_t d) {
  while ((UCSR0A & (1 << UDRE0)) == 0) {
//    if (!check_power()) return 0;
  }
  UDR0 = d;
  return 1;
}


uint8_t uart_reply(uint8_t d) {
  if (reply_count == 0) return 0;
  if (!uart_send(d)) return 0;
  reply_control_sum = _rol8(reply_control_sum ^ d);
  if ((--reply_count) == 0) {
    if (!uart_send(reply_control_sum)) return 0;
  }    
  return 1;
}

uint8_t uart_reply_pgm(PGM_VOID_P p_pgm, uint8_t size) {
  while (size--) {
    if (!uart_reply(pgm_read_byte(p_pgm++))) return 0;
  }
  return 1;
}

uint8_t uart_start_reply(uint8_t code, uint8_t paramlen) {
  reply_count = paramlen + 1;
  reply_control_sum = 0xFF;
  if (!uart_send(UARTCMD_REPLY_PREFIX)) return 0;
  if (!uart_send(reply_count)) return 0;
  return uart_reply(code);
}

