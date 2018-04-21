/*
 * tools.h
 *
 * Разные полезняшки
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef TOOLS_H_
#define TOOLS_H_

#define __VALUE_OF_MACRO_HELPER(x) #x
#define VALUE_OF_MACRO(x) __VALUE_OF_MACRO_HELPER(x)

#define __CONCAT2_HELPER(A, B) A ## B
#define CONCAT2(A, B) __CONCAT2_HELPER(A, B)
  
#define __CONCAT3_HELPER(A, B, C) A ## B ## C
#define CONCAT3(A, B, C) __CONCAT3_HELPER(A, B, C)


#define _GET_DDR(_X) DDR ## _X
#define _GET_PIN(_X) PIN ## _X
#define _GET_PORT(_X) PORT ## _X

#define DDR(_X) _GET_DDR(_X)
#define PIN(_X) _GET_PIN(_X)
#define PORT(_X) _GET_PORT(_X)

#define DDR_SET_OUTPUT(letter, bitno) DDR(letter) |= (1 << bitno)
#define DDR_SET_INPUT(letter, bitno) DDR(letter) &= ~(1 << bitno)

#define PORT_SET_HIGH(letter, bitno) PORT(letter) |= (1 << bitno)
#define PORT_SET_LOW(letter, bitno) PORT(letter) &= ~(1 << bitno)
  
#define PIN_READ(letter, bitno) (PIN(letter) & (1 << bitno)) 

#define array_size(x) (sizeof(x) / sizeof(x[0]))


#define carry_sub16(minuend, subtrahend) \
  ({ \
    uint8_t res; \
    asm ( \
      "sub %B[a], %B[b] \n" \
      "sbc %A[a], %A[b] \n" \
      "clr %[result] \n" \
      "adc %[result], __zero_reg__ \n" \
      : [result]"=r"(res), [a]"+r"(minuend) \
      : [b]"r"(subtrahend) \
    ); \
    res; \
  })


#endif /* TOOLS_H_ */