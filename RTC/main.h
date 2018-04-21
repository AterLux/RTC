/*
 * main.h
 *
 * Проект "Часы реального времени"
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef MAIN_H_
#define MAIN_H_

#ifndef F_CPU
  #error "F_CPU is not defined. Set F_CPU in Project properties -> Toolchain -> Symbols"
#endif  
//#define F_CPU 4000000UL

#if F_CPU != 4000000UL
  #warning "Is F_CPU not 4000000UL? Rly?"
#endif

#include <avr/io.h>

#define TARGET_TICK_PERIOD ((F_CPU + 128) / 256)


// Адреса EEPROM 0..7 зарезервированы для RTC (см. rtc.h)
// EEPROM addresses 0..7 are reserved for the RTC (see rtc.h)

// Адреса EEPROM 8..15 зарезервированы для pulling (см. pulling.h)
// EEPROM addresses 8..15 are reserved for the pulling (see pulling.h)


#define EEADDR_LANG 16
#define EEADDR_TEXT_FONT 17
#define EEADDR_DIGITS_FONT 18
#define EEADDR_ANIMATION 19

// Маски неиспользуемых пинов. На этих пинах будут включены подтягивающие резисторы
#define UNUSED_PINS_MASK_B 0b11111110
#define UNUSED_PINS_MASK_C 0b00001111
#define UNUSED_PINS_MASK_D 0b01100100


// Ячейка с калибровочным байтом используется также загрузчком.
// the calibration value EEPROM is also used by the bootloader
#define EEADDR_OSCAL (E2END - 1) 
// Адреса EEPROM E2END используется загрузчиком
// EEPROM address E2END is used by the bootloader


#define BOOTLOADER_SIZE_BYTES 512
#define BOTTLOADER_OFFSET (FLASHEND + 1L - BOOTLOADER_SIZE_BYTES)

#define jump_to_bootloader() ((void (*)(void))(BOTTLOADER_OFFSET / 2))()


#endif /* MAIN_H_ */