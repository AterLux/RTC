/*
 * ssd1306_commands.h
 *
 * Константы команд контроллера SSD1306
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef SSD1306_COMMANDS_H_
#define SSD1306_COMMANDS_H_


// Устанавливает контраст, значение - в следующем байте
#define CMD_SET_CONTRAST_CONTROL 0x81

#define CMD_ENTIRE_DISPLAY_ON(enabled) ((enabled) ? 0xA5 : 0xA4)

#define CMD_SET_INVERSE_DISPLAY(enabled) ((enabled) ? 0xA7 : 0xA6)

#define CMD_SET_DISPLAY_ON 0xAF
#define CMD_SET_DISPLAY_OFF 0xAE

// Настройка скроллинга, далее следуют 6 байт с параметрами
#define CMD_SCROLL_HORIZONTAL_RIGHT 0x26
#define CMD_SCROLL_HORIZONTAL_LEFT  0x27

// Настройка скроллинга, далее следуют 5 байт с параметрами
#define CMD_SCROLL_VERTICAL_AND_HORIZONTAL_RIGHT 0x29
#define CMD_SCROLL_VERTICAL_AND_HORIZONTAL_LEFT  0x2A

#define CMD_SCROLL_DISABLE 0x2E
#define CMD_SCROLL_ENABLE 0x2F

#define CMD_SCROLL_SET_VERTICAL_AREA 0xA3

#define CMD_SET_COLUMN 0xA3

// Команды установки адреса в режиме страничной адресации
#define CMD_SET_COLUMN_LO(x) ((x) & 0x0F)
#define CMD_SET_COLUMN_HI(x) (0x10 | ((x) & 0x0F))

// Выбор режима адресации, далее следует 1 байт с параметрами (один из MEMORY_ADDRESSING_MODE_...)
#define CMD_SET_MEMORY_ADDRESSING_MODE 0x20

#define MEMORY_ADDRESSING_MODE_HORIZONTAL 0x20
#define MEMORY_ADDRESSING_MODE_VERTICAL 0x21
#define MEMORY_ADDRESSING_MODE_PAGE 0x22

// Команда установки горизонтального диапазона в горизонтальном и вертикальном режиме, далее следуют 2 байта с параметрами
#define CMD_SET_COULUMN_START_END_ADDRESS 0x21

// Команда установки вертикального диапазона в горизонтальном и вертикальном режиме, далее следуют 2 байта с параметрами
#define CMD_SET_PAGE_START_END_ADDRESS 0x22

// Команды установки страницы в режиме страничной адресации
#define CMD_SET_PAGE(x) (0xB0 | ((x) & 0x07))

// Устанавливает адрес верхней строки дисплея
#define CMD_SET_DISPLAY_START_LINE(x) (0x40 | ((x) & 0x3F))

// Устанавливает горизонтальное отражение изображения
#define CMD_SET_SEGMENT_REMAP(remapped) ((remapped) ? 0xA1 : 0xA0)

// Установка мультиплекора генератора изображения (количество используемых выводов), далее следует 1 байт с параметром
#define CMD_SET_MUX_RATIO 0xA8

// Устанавливает вертикальное отражение изображения
#define CMD_SET_SCAN_DIRECTION(remapped) ((remapped) ? 0xC8 : 0xC0)

// Установка позиции вертикального прокручивания, далее следует 1 байт с параметром
#define CMD_SET_DISPLAY_OFFSET 0xD3

// Установка переназначения общих выводов, далее следует 1 байт с параметром COM_PINS_CONFIGURATION
#define CMD_SET_COM_PINS_CONFIGURATION 0xDA
#define COM_PINS_CONFIGURATION(alternative, left_right_remap) (((alternative) ? 0x10 : 0x00) | ((left_right_remap) ? 0x20 : 0x00) | 0x02)

// Установка частоты и делителя осциллятора, далее следует 1 байт с параметрами CLOCK_RATIO_FREQUENCY
#define CMD_SET_CLOCK_RATIO_FREQUENCY 0xD5
#define CLOCK_RATIO_FREQUENCY(ratio, freq) ((((ratio) - 1) &  0x0F) | (((freq) & 0x0F) << 4)) 

// Установка периода предзаряда, далее следует 1 байт с параметрами PRECHARGE_PERIOD
#define CMD_SET_PRECHARGE_PERIOD 0xD9
#define PRECHARGE_PERIOD(phase1, phase2) (((phase1) &  0x0F) | (((phase2) & 0x0F) << 4)) 

// Установка периода отпускания VCOMMH, далее следует 1 байт с параметром, один из VCOMH_DESELECT_...
#define CMD_SET_VCOMH_DESELECT 0xDB
#define VCOMH_DESELECT_0_65_VCC 0x00
#define VCOMH_DESELECT_0_77_VCC 0x20
#define VCOMH_DESELECT_0_83_VCC 0x30


// Включение/отключение повышающего преобразователя, далее следует 1 байт с параметром, один из CHARGE_PUMP_..
#define CMD_CHARGE_PUMP_CONTROL 0x8D
#define CHARGE_PUMP_ENABLE 0x14
#define CHARGE_PUMP_DISABLE 0x10

#define CMD_NOP 0xE3



#endif /* SSD1306_COMMANDS_H_ */