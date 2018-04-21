/*
 * SSD1306.c
 *
 * Драйвер контроллера SSD1306 (для OLED дисплеев 128x64, 128x32)
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 



#include <avr/io.h>

#include "display.h"


#ifdef SSD1306
// Исходный код компилируется только если в display.h подключен ssd1306.h

#include "ssd1306_commands.h"

#if !defined(F_CPU) && defined(DISPLAYHW_F_CPU) 
  #define F_CPU DISPLAYHW_F_CPU
#endif

#include <util/delay.h>
#include <avr/pgmspace.h>




/*
inline void ssd1306_cs_assert() {
  SSD1306_CS_PORT &= ~SSD1306_CS;
}

inline void ssd1306_cs_deassert() {
  SSD1306_CS_PORT |= SSD1306_CS;
}
*/
/*void inline ssd1306_sw_send(uint8_t data, uint8_t a0) {
  if (a0) {
    SSD1306_A0_PORT |= SSD1306_A0;
  } else {
    SSD1306_A0_PORT &= ~SSD1306_A0;
  }
  ssd1306_cs_assert();
  for (uint8_t i = 0; i < 8; i++) {
    SSD1306_SCL_PORT &= ~SSD1306_SCL;
    if (data & 0x80) {
      SSD1306_SI_PORT |= SSD1306_SI;
    } else {
      SSD1306_SI_PORT &= ~SSD1306_SI;
    }
    data <<= 1;
    SSD1306_SCL_PORT |= SSD1306_SCL;
  }
  ssd1306_cs_deassert();
}
*/
/*
void display_end() {
  while (!(SPSR & (1 << SPIF))); // Ожидаем завершения передачи
  SSD1306_CS_PORT |= SSD1306_CS; // Отпускаем CS
}

void display_data(uint8_t x) {
  if (SSD1306_CS_PORT & SSD1306_CS) { // Если CS не дёрнут, то начинаем передачу
    SSD1306_A0_PORT |= SSD1306_A0;
    SSD1306_CS_PORT &= ~SSD1306_CS;
    SPDR = x;
  } else {
    while (!(SPSR & (1 << SPIF)));
    SSD1306_A0_PORT |= SSD1306_A0;
    SPDR = x;
  }  
}

void display_cmd(uint8_t x) {
  if (SSD1306_CS_PORT & SSD1306_CS) { // Если CS не дёрнут, то начинаем передачу
    SSD1306_A0_PORT &= ~SSD1306_A0;
    SSD1306_CS_PORT &= ~SSD1306_CS;
    SPDR = x;
  } else {
    while (!(SPSR & (1 << SPIF)));
    SSD1306_A0_PORT &= ~SSD1306_A0;
    SPDR = x;
  }  
}
*/

//uint8_t display


// Инициализирует дисплей
uint8_t display_init() {
  if (!displayhw_init()) return 0;
  
  if (!safe_delay_ms(5)) return 0;

  
  for (uint8_t i = 0 ; i < 6 ; i++) {
    if (!display_cmd(CMD_NOP)) return 0; // NOP
  }
  if (!display_end()) return 0;
  if (
       !display_cmd(CMD_SET_DISPLAY_OFF)
    || !display_end()
   ) return 0;
   
  if (!safe_delay_ms(20)) return 0;

  if (
       !display_cmd(CMD_SCROLL_DISABLE) // deactivate scroll
    || !display_cmd2(CMD_SET_MUX_RATIO, DISPLAY_HEIGHT - 1) // multiplex ratio
    || !display_cmd2(CMD_SET_DISPLAY_OFFSET, SSD1306_DISPLAY_OFFSET) // display offset
    
    || !display_cmd(CMD_SET_DISPLAY_START_LINE(0)) // display start line 0 
    || !display_cmd(CMD_SET_SEGMENT_REMAP(SSD1306_FLIP_HORIZONTALLY)) // rotation - right 
    || !display_cmd(CMD_SET_SCAN_DIRECTION(SSD1306_FLIP_VERTICALLY))  // com scan dir
    || !display_cmd2(CMD_SET_COM_PINS_CONFIGURATION, COM_PINS_CONFIGURATION(SSD1306_COM_PINS_INTERLEAVED, SSD1306_COM_HALVES_CHANGED)) // com seq/alt 
    
    || !display_cmd2(CMD_SET_CONTRAST_CONTROL, SSD1306_CONTRAST_INITIAL_VALUE) // contrast set
    || !display_cmd(CMD_ENTIRE_DISPLAY_ON(0)) // non-entire display on
    || !display_cmd(CMD_SET_INVERSE_DISPLAY(0)) // non-inverting
    
    || !display_cmd2(CMD_SET_CLOCK_RATIO_FREQUENCY, CLOCK_RATIO_FREQUENCY(SSD1306_CLOCK_RATIO, SSD1306_OSC_FREQUENCY)) // display divide ratio 
    
    || !display_cmd2(CMD_SET_PRECHARGE_PERIOD, PRECHARGE_PERIOD(SSD1306_PRECHARGE_PHASE1_DURATION, SSD1306_PRECHARGE_PHASE2_DURATION))  // precharge period
    

    || !display_cmd2(CMD_SET_MEMORY_ADDRESSING_MODE, MEMORY_ADDRESSING_MODE_PAGE) // memory addressing: page addressing
    || !display_cmd(CMD_SET_PAGE(0)) // page address 
    || !display_cmd(CMD_SET_COULUMN_START_END_ADDRESS) || !display_cmd2(0, DISPLAY_WIDTH - 1) // column address (min, max);
    || !display_cmd(CMD_SET_PAGE_START_END_ADDRESS) || !display_cmd2(0, DISPLAY_PAGES - 1) // page address (min, max);


    || !display_cmd2(CMD_SET_VCOMH_DESELECT, VCOMH_DESELECT_0_77_VCC) // vcom deselect level 0.77 x Vcc
//    || !display_cmd(0x32) // vpp value set 3

//  display_cmd2(0xAD, 0x8B); // dc-dc turn on 10
    || !display_cmd2(CMD_CHARGE_PUMP_CONTROL, (SSD1306_USE_CHARGE_PUMP) ? CHARGE_PUMP_ENABLE : CHARGE_PUMP_DISABLE) // dc-dc turn on
    || !display_end()
    ) return 0;

  if (!display_clear()) return 0;
  
  if (
       !display_cmd(CMD_SET_DISPLAY_ON) // display turn on 11
    || !display_end()
    ) return 0;
  return 1;
} 

// Готовит контроллер дисплея и затем переводит его в режим экономии энергии
uint8_t display_powerdown() {
  return display_cmd(CMD_SET_DISPLAY_OFF) && display_end();
}

uint8_t display_resume_power() {
  return display_cmd(CMD_SET_DISPLAY_ON) && display_end();
}

uint8_t display_set_contrast(uint8_t x) {
  return display_cmd2(CMD_SET_CONTRAST_CONTROL, x) && display_end();
}

// Выбирает страницу (позицию по вертикали) для вывода
uint8_t display_setpage(uint8_t page) {
  return display_cmd(CMD_SET_PAGE(page));
}

// Выбирает горизонтальную позицию для вывода
uint8_t display_setx(uint8_t x) {
  return display_cmd2(CMD_SET_COLUMN_HI(x >> 4), CMD_SET_COLUMN_LO(x));
}

// Выбирает страницу и горизонтальную позицию для вывода
uint8_t display_setpos(uint8_t page, uint8_t x) {
  return display_cmd(CMD_SET_PAGE(page))
      && display_cmd2(CMD_SET_COLUMN_HI(x >> 4), CMD_SET_COLUMN_LO(x));
}

#endif /* SSD1306 */