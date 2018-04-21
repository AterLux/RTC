/*
 * displayhw.c
 *
 * Здесь реализуются некоторые стандартные интерфейсы подключения к различным монохромным дисплеям
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 

#include "display.h"


// Функции, реализуемые в этом файле, появляются только тогда, когда в display.h подключен заголовок какого-либо драйвера дисплея, который уже подключает displayhw.h
// Если же драйвер дисплея использует свои реализации интерфейса, то он самостоятельно реализует и функции доступа
#ifdef DISPLAYHW  

#include <util/twi.h>


#define HW_STATUS_NONE 0
#define HW_STATUS_STARTED 1
#define HW_STATUS_DATA 2


#define _GET_DDR(_X) DDR ## _X
#define _GET_PIN(_X) PIN ## _X
#define _GET_PORT(_X) PORT ## _X

#define DDR(_X) _GET_DDR(_X)
#define PIN(_X) _GET_PIN(_X)
#define PORT(_X) _GET_PORT(_X)



#ifdef DISPLAYHW_MODE_I2C_SOFTWARE
  
  #define __REQUIRED_HALF_CYCLE_DELAY ((DISPLAYHW_F_CPU - 1) / I2C_BITRATE / 2)

#endif

#ifdef DISPLAY_HW_MODE_SPI_SOFTWARE

  #define __REQUIRED_HALF_CYCLE_DELAY ((DISPLAYHW_F_CPU - 1) / SPI_BITRATE / 2)

#endif


#ifdef __REQUIRED_HALF_CYCLE_DELAY

  #if (__REQUIRED_HALF_CYCLE_DELAY < 2 )
    #define half_cycle_delay() 
  #elif (__REQUIRED_HALF_CYCLE_DELAY < 3 )
    #define half_cycle_delay() asm volatile ("nop")
  #elif (__REQUIRED_HALF_CYCLE_DELAY < 4 )
    #define half_cycle_delay() asm volatile ("rjmp .+0")
  #elif (__REQUIRED_HALF_CYCLE_DELAY < 6 )
    #define half_cycle_delay() asm volatile ("rjmp .+0\n rjmp .+0")
  #elif (__REQUIRED_HALF_CYCLE_DELAY < 8 )
    #define half_cycle_delay() asm volatile ("rjmp .+0\n rjmp .+0\n rjmp .+0")
  #elif (__REQUIRED_HALF_CYCLE_DELAY < 768)
    #define half_cycle_delay() \
       { \
       uint8_t t ; \
       asm volatile ( \
              "ldi %0, %1 \n" \
         "1:" "dec %0 \n" \
              "brne 1b \n" \
         : "=&d" (t) \
         : "M" ((__REQUIRED_HALF_CYCLE_DELAY - 1) / 3) \
      ); \
      }
  #else
    #define half_cycle_delay() \
       { \
       uint16_t t; \
       asm volatile ( \
              "ldi %A0, %1 \n" \
              "ldi %B0, %2 \n" \
         "1:" "sbiw %0, 1 \n" \
              "brne 1b \n" \
         : "=&w" (t) \
         : "M" ((__REQUIRED_HALF_CYCLE_DELAY - 2) / 4) & 0xFF), \
           "M" ((__REQUIRED_HALF_CYCLE_DELAY - 2) / 4) >> 8) \
      ); \
      }
         
  #endif
  

#endif /* __REQUIRED_HALF_CYCLE_DELAY */


#ifdef DISPLAYHW_MODE_I2C_TWI

/******************/
/* Аппаратный I2C */
/******************/

#define __I2C_UNSCALED_TWBR_VALUE (((DISPLAYHW_F_CPU - 1) / I2C_BITRATE - 14) / 2)

#if (((I2C_F_CPU + I2C_BITRATE / 2) / I2C_BITRATE) <= 16) || (__I2C_UNSCALED_TWBR_VALUE < 1)
  #define __I2C_DEFAULT_TWSR 0
  #define __I2C_TWBR 0
#elif (__I2C_UNSCALED_TWBR_VALUE < 256)
  #define __I2C_DEFAULT_TWSR 0
  #define __I2C_TWBR __I2C_UNSCALED_TWBR_VALUE
#elif (__I2C_UNSCALED_TWBR_VALUE < 1024)
  #define __I2C_DEFAULT_TWSR (1 << TWPS0)
  #define __I2C_TWBR (__I2C_UNSCALED_TWBR_VALUE / 4)
#elif (__I2C_UNSCALED_TWBR_VALUE < 4096)
  #define __I2C_DEFAULT_TWSR (1 << TWPS1)
  #define __I2C_TWBR (__I2C_UNSCALED_TWBR_VALUE / 16)
#elif (__I2C_UNSCALED_TWBR_VALUE < 16384)
  #define __I2C_DEFAULT_TWSR ((1 << TWPS1) | (1 << TWPS0)
  #define __I2C_TWBR (__I2C_UNSCALED_TWBR_VALUE / 64)
#else
  #error "I2C_BITRATE value is too low!"
#endif



static uint8_t wait_i2c() {
  uint16_t anti_halt = 0;
  while (!(TWCR & (1 << TWINT))) { // Ждём появления бита прерывания 
    if (check_abort()) return 0;
    if (++anti_halt == 0) return 0; // Считаем итерации. Когда счётчик переполнится - выходим с ошибкой
  }
  return TWSR & TW_STATUS_MASK;
}

/* Посылает сигнал STOP, освобождая шину I2C 
 * возвращает 1, если операция прошла успешно, 0 - может указывать на зависание шины */
uint8_t i2c_stop() {
  uint8_t r = wait_i2c();
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  return r;
}

/* Посылает сигнал START, пытается восстановиться, если ошибка шины, высылает sla+w, 
 * sla - адрес адресуемого устройства (0 - 127)
 * is_write - 0 - для операции чтения, любое другое значение - для операции записи
 * возвращает 1, если операции прошли успешно и получен ACK, 0 - в противном случае */
uint8_t i2c_start() {
  uint16_t anti_halt = 0;
  while (TWCR & (1 << TWSTO)) { // Ждём пропадания бита остановки
    if (check_abort()) return 0;
    if (++anti_halt == 0) { // Считаем итерации. Когда счётчик переполнится - выходим с ошибкой
      return 0; 
    }      
  }
  
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  uint8_t sr = wait_i2c();
  if ((sr != TW_REP_START) && (sr != TW_START)) {
    if (check_abort()) return 0;
    TWCR = (1 << TWINT); // Отключаем TWI, чтобы восстановиться
    asm volatile ("rjmp .+0"); // пауза два такта, так, на всякий случай
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    sr = wait_i2c();
    if ((sr != TW_START) && (sr != TW_REP_START)) {
      return 0; // Если и второй раз не удалось выдать импульс старт - что делать, выходим
    }      
  }
  
  TWDR = (DISPLAY_I2C_ADDR << 1) | TW_WRITE;
  TWCR = (1 << TWINT) | (1 << TWEN);
  return 1;
}

/* Отправляет 1 байт по шине I2C. 
 * Возвращает 1, если получен ACK, 0 - в противном случае
 */
uint8_t i2c_send(uint8_t b) {
  if (!(TWCR & (1 << TWINT))) {
    uint16_t anti_halt = 0;
    while (!(TWCR & (1 << TWINT))) { // Ждём появления бита прерывания 
      if (check_abort()) return 0;
      if (++anti_halt == 0) return 0; // Считаем итерации. Когда счётчик переполнится - выходим с ошибкой
    }
  }    
  uint8_t stat = TWSR;
  
  TWDR = b;
  TWCR = (1 << TWINT) | (1 << TWEN);
  stat &= TW_STATUS_MASK;
  return ((stat == TW_MT_DATA_ACK) || (stat == TW_MT_SLA_ACK));
}

static uint8_t displayhw_status;

uint8_t displayhw_init() {
  displayhw_status = HW_STATUS_NONE;
  TWSR = __I2C_DEFAULT_TWSR; 
  TWBR = __I2C_TWBR; 
  TWCR = (1 << TWEN);
  return 1;
}

void displayhw_shutdown() {
  TWCR = 0;
}


uint8_t display_data(uint8_t d) {
  if (displayhw_status == HW_STATUS_NONE) {
    if (!i2c_start()) return 0;
  }
  if (displayhw_status != HW_STATUS_DATA) {
    if (!i2c_send(0x40)) {
      i2c_stop();
      displayhw_status = HW_STATUS_NONE;
      return 0;
    }      
    displayhw_status = HW_STATUS_DATA;
  }
  if (!i2c_send(d)) {
    i2c_stop();
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }   
  return 1;   
}

uint8_t display_cmd(uint8_t c) {
  if (displayhw_status != HW_STATUS_STARTED) {
    if (!i2c_start()) return 0;
  }
  displayhw_status = HW_STATUS_STARTED;
  if (!i2c_send(0x80)) {
    i2c_stop();
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }      
  if (!i2c_send(c)) {
    i2c_stop();
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }   
  return 1;   
}

uint8_t display_end() {
  if (displayhw_status == HW_STATUS_NONE) return 1;
  uint8_t stat = wait_i2c();
  displayhw_status = HW_STATUS_NONE;
  return i2c_stop() && ((stat == TW_MT_DATA_ACK) || (stat == TW_MT_SLA_ACK));
}

#endif /* DISPLAYHW_MODE_TWI */




#ifdef DISPLAYHW_MODE_I2C_SOFTWARE
/*******************/
/* Программный I2C */
/*******************/

static inline void i2c_scl_down() {
  DDR(SOFTI2C_SCL_PORT_LETTER) |= (1 << SOFTI2C_SCL_PIN_NUM);
}

static inline void i2c_scl_up() {
  DDR(SOFTI2C_SCL_PORT_LETTER) &= ~(1 << SOFTI2C_SCL_PIN_NUM);
}

static inline void i2c_sda_down() {
  DDR(SOFTI2C_SDA_PORT_LETTER) |= (1 << SOFTI2C_SDA_PIN_NUM);
}

static inline void i2c_sda_up() {
  DDR(SOFTI2C_SDA_PORT_LETTER) &= ~(1 << SOFTI2C_SDA_PIN_NUM);
}

static inline uint8_t i2c_read_sda() {
  return PIN(SOFTI2C_SDA_PORT_LETTER) & (1 << SOFTI2C_SDA_PIN_NUM);
}

static inline uint8_t i2c_read_scl() {
  return PIN(SOFTI2C_SCL_PORT_LETTER) & (1 << SOFTI2C_SCL_PIN_NUM);
}

static uint8_t i2c_wait_scl_up() {
  i2c_scl_up();
  if (!i2c_read_scl()) {
    half_cycle_delay();
    if (check_abort() || !i2c_read_scl()) {
      i2c_sda_up();
      return 0;
    }      
  }
  return 1;
}

static uint8_t i2c_send(uint8_t b) {
  for (uint8_t i = 8 ; i > 0 ; i--) {
    if (b & 0x80) {
      i2c_sda_up();
    } else {
      i2c_sda_down();
    }
    b <<= 1;
    half_cycle_delay();
    i2c_scl_up();
    if (!i2c_wait_scl_up()) return 0;
    half_cycle_delay();
    i2c_scl_down();
  }
  i2c_sda_up();
  half_cycle_delay();
  i2c_scl_up();
  if (!i2c_wait_scl_up()) return 0;
  uint8_t res = (i2c_read_sda() == 0);
  half_cycle_delay();
  i2c_scl_down();
  return res;
}

static uint8_t i2c_start() {
  i2c_scl_up();
  if (check_abort()) return 0;
  if (!i2c_wait_scl_up()) return 0;
  half_cycle_delay();
  i2c_sda_down();
  half_cycle_delay();
  i2c_scl_down();
  return i2c_send((DISPLAY_I2C_ADDR << 1) | TW_WRITE);
}

static uint8_t i2c_stop() {
  i2c_sda_down();
  half_cycle_delay();
  i2c_scl_up();
  half_cycle_delay();
  i2c_sda_up();
  half_cycle_delay();
  return 1;
}  

uint8_t displayhw_init() {
  DDR(SOFTI2C_SCL_PORT_LETTER) &= ~(1 << SOFTI2C_SCL_PIN_NUM);
  PORT(SOFTI2C_SCL_PORT_LETTER) &= ~(1 << SOFTI2C_SCL_PIN_NUM);
  DDR(SOFTI2C_SDA_PORT_LETTER) &= ~(1 << SOFTI2C_SDA_PIN_NUM);
  PORT(SOFTI2C_SDA_PORT_LETTER) &= ~(1 << SOFTI2C_SDA_PIN_NUM);
  return 1;
}

void displayhw_shutdown() {
  DDR(SOFTI2C_SCL_PORT_LETTER) &= ~(1 << SOFTI2C_SCL_PIN_NUM);
  PORT(SOFTI2C_SCL_PORT_LETTER) &= ~(1 << SOFTI2C_SCL_PIN_NUM);
  DDR(SOFTI2C_SDA_PORT_LETTER) &= ~(1 << SOFTI2C_SDA_PIN_NUM);
  PORT(SOFTI2C_SDA_PORT_LETTER) &= ~(1 << SOFTI2C_SDA_PIN_NUM);
}

static uint8_t displayhw_status;

uint8_t display_data(uint8_t d) {
  if (displayhw_status == HW_STATUS_NONE) {
    if (!i2c_start()) return 0;
  }
  if (displayhw_status != HW_STATUS_DATA) {
    if (!i2c_send(0x40)) {
      displayhw_status = HW_STATUS_NONE;
      return 0;
    }      
    displayhw_status = HW_STATUS_DATA;
  }
  if (!i2c_send(d)) {
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }   
  return 1;   
}

uint8_t display_cmd(uint8_t c) {
  if (displayhw_status != HW_STATUS_STARTED) {
    if (!i2c_start()) return 0;
  }
  displayhw_status = HW_STATUS_STARTED;
  if (!i2c_send(0x80)) {
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }      
  if (!i2c_send(c)) {
    displayhw_status = HW_STATUS_NONE;
    return 0;
  }   
  return 1;   
}

uint8_t display_end() {
  if (displayhw_status != HW_STATUS_NONE) {
     displayhw_status = HW_STATUS_NONE;
     return i2c_stop();
  }   
  return 1;
}  


#endif /* DISPLAYHW_MODE_I2C_SOFTWARE */


#if defined(DISPLAYHW_MODE_SPI_SOFTWARE)  || defined(DISPLAYHW_MODE_SPI) || defined(DISPLAYHW_MODE_SPI_USART)

/********************************/
/* Программный и аппаратный SPI */
/********************************/

static inline void spi_cs_assert() {
  PORT(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
}

static inline void spi_cs_deassert() {
  PORT(SPI_CS_PORT_LETTER) |= (1 << SPI_CS_PIN_NUM);
}

static inline void spi_dc_data() {
  PORT(SPI_DC_PORT_LETTER) |= (1 << SPI_DC_PIN_NUM);
}

static inline void spi_dc_command() {
  PORT(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
}

static inline uint8_t spi_cs_is_deasserted() {
  return PORT(SPI_CS_PORT_LETTER) & (1 << SPI_CS_PIN_NUM);
}

static inline uint8_t spi_dc_is_data() {
  return PORT(SPI_DC_PORT_LETTER) & (1 << SPI_DC_PIN_NUM);
}

#endif /* defined(DISPLAYHW_MODE_SPI_SOFTWARE)  || defined(DISPLAYHW_MODE_SPI) || defined(DISPLAYHW_MODE_SPI_USART) */


#ifdef DISPLAYHW_MODE_SPI

/*******************/
/* Аппаратный SPI */
/*******************/

#define __SPI_DIVIDER ((DISPLAYHW_F_CPU - 1) / SPI_BITRATE + 1)

#if __SPI_DIVIDER <= 2
  #define __SPCR_SPEED ((0 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 4
  #define __SPCR_SPEED ((0 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#elif __SPI_DIVIDER <= 8
  #define __SPCR_SPEED ((0 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 16
  #define __SPCR_SPEED ((0 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#elif __SPI_DIVIDER <= 32
  #define __SPCR_SPEED ((1 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 64
  #define __SPCR_SPEED ((1 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#else
  #define __SPCR_SPEED ((1 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#endif


uint8_t displayhw_init() {
  spi_cs_deassert();
  DDR(SPI_CS_PORT_LETTER) |= (1 << SPI_CS_PIN_NUM);
  spi_dc_command();
  DDR(SPI_DC_PORT_LETTER) |= (1 << SPI_DC_PIN_NUM);

  DDR(SPI_PORT_LETTER) |= (1 << SPI_MOSI_PIN_NUM);
  DDR(SPI_PORT_LETTER) |= (1 << SPI_SCK_PIN_NUM);
  
  SPCR = __SPCR_SPEED | (1 << SPE) | (1 << MSTR);
  SPSR = __SPSR_SPEED;
  return 1;
}

void displayhw_shutdown() {
  SPCR = 0;
  SPSR = 0;
  DDR(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  PORT(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  DDR(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  PORT(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  DDR(SPI_PORT_LETTER) &= ~(1 << SPI_MOSI_PIN_NUM);
  PORT(SPI_PORT_LETTER) &= ~(1 << SPI_MOSI_PIN_NUM);
  DDR(SPI_PORT_LETTER) &= ~(1 << SPI_SCK_PIN_NUM);
  PORT(SPI_PORT_LETTER) &= ~(1 << SPI_SCK_PIN_NUM);
}


uint8_t display_data(uint8_t d) {
  if (spi_cs_is_deasserted()) {
    spi_cs_assert();
  } else {  
    while ((SPSR & (1 << SPIF)) == 0) {
      if ((SPCR & (1 << SPE)) == 0) return 0;
    }
  }
  spi_dc_data();
  SPDR = d;
  return 1;
}

uint8_t display_cmd(uint8_t c) {
  if (spi_cs_is_deasserted()) {
    spi_cs_assert();
  } else {  
    while ((SPSR & (1 << SPIF)) == 0) {
      if ((SPCR & (1 << SPE)) == 0) return 0;
    }
  }
  spi_dc_command();
  SPDR = c;
  return 1;
}

uint8_t display_end() {
  if (!spi_cs_is_deasserted()) {
    while ((SPSR & (1 << SPIF)) == 0) {
      if ((SPCR & (1 << SPE)) == 0) return 0;
    }
    spi_dc_command();
    spi_cs_deassert();
  }
  return 1;
}

#endif /* DISPLAYHW_MODE_SPI */

#ifdef DISPLAYHW_MODE_SPI_USART

/****************************************************/
/* SPI посредством модуля USART в режиме SPI-Master */
/****************************************************/

#define _GET_UM(_X, _Y) _X ## _Y
#define _UM2(_X, _Y) _GET_UM(_X, _Y)
#define _UM(_X) _UM2(_X, DISPLAY_USART_NUM)

#define _GET_UMP(_X, _Y, _P) _X ## _Y ## _P
#define _UMP2(_X, _Y, _P) _GET_UMP(_X, _Y, _P)
#define _UMP(_X, _P) _UMP2(_X, DISPLAY_USART_NUM, _P)

#define __SPI_DIVIDER ((DISPLAYHW_F_CPU - 1) / SPI_BITRATE + 1)

#if __SPI_DIVIDER <= 2
  #define __SPCR_SPEED ((0 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 4
  #define __SPCR_SPEED ((0 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#elif __SPI_DIVIDER <= 8
  #define __SPCR_SPEED ((0 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 16
  #define __SPCR_SPEED ((0 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#elif __SPI_DIVIDER <= 32
  #define __SPCR_SPEED ((1 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (1 << SPI2X)
#elif __SPI_DIVIDER <= 64
  #define __SPCR_SPEED ((1 << SPR1) | (0 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#else
  #define __SPCR_SPEED ((1 << SPR1) | (1 << SPR0))
  #define __SPSR_SPEED (0 << SPI2X)
#endif

#define __SPI_USART_UBRR ((DISPLAYHW_F_CPU - 1) / SPI_BITRATE / 2)

uint8_t displayhw_init() {
  spi_cs_deassert();
  DDR(SPI_CS_PORT_LETTER) |= (1 << SPI_CS_PIN_NUM);
  spi_dc_command();
  DDR(SPI_DC_PORT_LETTER) |= (1 << SPI_DC_PIN_NUM);

  DDR(DISPLAY_USART_PORT_LETTER) |= (1 << DISPLAY_USART_XCK_PIN_NUM);
  _UMP(UCSR, C) = (1 << _UMP(UMSEL, 1)) | (1 << _UMP(UMSEL, 0));
  _UM(UBRR) = __SPI_USART_UBRR;
  _UMP(UCSR, B) = (1 << _UM(TXEN));
  
  return 1;
}

void displayhw_shutdown() {
  _UMP(UCSR, B) = 0;
  DDR(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  PORT(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  DDR(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  PORT(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  DDR(DISPLAY_USART_PORT_LETTER) &= ~(1 << DISPLAY_USART_XCK_PIN_NUM);
  PORT(DISPLAY_USART_PORT_LETTER) &= ~(1 << DISPLAY_USART_XCK_PIN_NUM);
}


uint8_t display_data(uint8_t d) {
  if (spi_cs_is_deasserted()) {
    spi_cs_assert();
    spi_dc_data();
  } else if (!spi_dc_is_data()) {  
    // Если идёт передача, но передаются не данные, ждём полного завершения передачи и меням значение D/C#
    while ((_UMP(UCSR, A) & (1 << _UM(TXC))) == 0) {
      if ((_UMP(UCSR, B) & (1 << _UM(TXEN))) == 0) return 0;
    }
    spi_dc_data();
  } else {
    // Если идёт передача и передаются данные, просто ждём освобождения буфера
    while ((_UMP(UCSR, A) & (1 << _UM(UDRE))) == 0) {
      if ((_UMP(UCSR, B) & (1 << _UM(TXEN))) == 0) return 0;
    }
  }
  _UM(UDR) = d;
  return 1;
}

uint8_t display_cmd(uint8_t c) {
  if (spi_cs_is_deasserted()) {
    spi_cs_assert();
    spi_dc_command();
  } else if (spi_dc_is_data()) {  
    // Если идёт передача, но передаётся не команда, ждём полного завершения передачи и меням значение D/C#
    while ((_UMP(UCSR, A) & (1 << _UM(TXC))) == 0) {
      if ((_UMP(UCSR, B) & (1 << _UM(TXEN))) == 0) return 0;
    }
    spi_dc_command();
  } else {
    // Если идёт передача и передаётся команда, просто ждём освобождения буфера
    while ((_UMP(UCSR, A) & (1 << _UM(UDRE))) == 0) {
      if ((_UMP(UCSR, B) & (1 << _UM(TXEN))) == 0) return 0;
    }
  }
  _UM(UDR) = c;
  return 1;
}

uint8_t display_end() {
  if (!spi_cs_is_deasserted()) {
    while ((_UMP(UCSR, A) & (1 << _UM(TXC))) == 0) {
      if ((_UMP(UCSR, B) & (1 << _UM(TXEN))) == 0) return 0;
    }
    spi_dc_command();
    spi_cs_deassert();
  }
  return 1;
}

#endif /* DISPLAYHW_MODE_SPI */



#ifdef DISPLAYHW_MODE_SPI_SOFTWARE

/*******************/
/* Программный SPI */
/*******************/


static inline void spi_sclk_low() {
  PORT(SOFTSPI_SCLK_PORT_LETTER) &= ~(1 << SOFTSPI_SCLK_PIN_NUM);
}

static inline void spi_sclk_high() {
  PORT(SOFTSPI_SCLK_PORT_LETTER) |= (1 << SOFTSPI_SCLK_PIN_NUM);
}

static inline void spi_sdat_low() {
  PORT(SOFTSPI_SDAT_PORT_LETTER) &= ~(1 << SOFTSPI_SDAT_PIN_NUM);
}

static inline void spi_sdat_high() {
  PORT(SOFTSPI_SDAT_PORT_LETTER) |= (1 << SOFTSPI_SDAT_PIN_NUM);
}

uint8_t displayhw_init() {
  spi_cs_deassert();
  DDR(SPI_CS_PORT_LETTER) |= (1 << SPI_CS_PIN_NUM);
  spi_dc_command();
  DDR(SPI_DC_PORT_LETTER) |= (1 << SPI_DC_PIN_NUM);
  spi_sclk_low();
  DDR(SOFTSPI_SCLK_PORT_LETTER) |= (1 << SOFTSPI_SCLK_PIN_NUM);
  spi_sdat_low();
  DDR(SOFTSPI_SDAT_PORT_LETTER) |= (1 << SOFTSPI_SDAT_PIN_NUM);
  return 1;
}

void displayhw_shutdown() {
  DDR(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  PORT(SPI_CS_PORT_LETTER) &= ~(1 << SPI_CS_PIN_NUM);
  DDR(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  PORT(SPI_DC_PORT_LETTER) &= ~(1 << SPI_DC_PIN_NUM);
  DDR(SOFTSPI_SCLK_PORT_LETTER) &= ~(1 << SOFTSPI_SCLK_PIN_NUM);
  PORT(SOFTSPI_SCLK_PORT_LETTER) &= ~(1 << SOFTSPI_SCLK_PIN_NUM);
  DDR(SOFTSPI_SDAT_PORT_LETTER) &= ~(1 << SOFTSPI_SDAT_PIN_NUM);
  PORT(SOFTSPI_SDAT_PORT_LETTER) &= ~(1 << SOFTSPI_SDAT_PIN_NUM);
}


static uint8_t spi_send(uint8_t d) {
  for (uint8_t i = 8 ; i != 0 ; i--) {
    if (d & 0x80) {
      spi_sdat_high();
    } else {
      spi_sdat_low();
    }
    d <<= 1;
    half_cycle_delay();
    spi_sclk_high();
    half_cycle_delay();
    spi_sclk_low();
  }
  return 1;
}

uint8_t display_data(uint8_t d) {
  spi_cs_assert();
  spi_dc_data();
  return spi_send(d);
}  

uint8_t display_cmd(uint8_t c) {
  spi_cs_assert();
  spi_dc_command();
  return spi_send(c);
}

uint8_t display_end() {
  spi_sdat_low();
  spi_cs_deassert();
  return 1;
}  

#endif /* DISPLAYHW_MODE_SPI_SOFTWARE */



uint8_t display_cmd2(uint8_t c1, uint8_t c2) {
  return display_cmd(c1) && display_cmd(c2);
};

#endif