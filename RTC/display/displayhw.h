/*
 * displayhw.h
 *
 * Здесь реализуются некоторые стандартные интерфейсы подключения к различным монохромным дисплеям
 * 
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef DISPLAYHW
#define DISPLAYHW

#include <avr/io.h>

// Тактовая частота процессора 
// CPU frequency
#ifdef F_CPU
  #define DISPLAYHW_F_CPU F_CPU
#else
  #error "F_CPU is not defined. Set F_CPU in Project properties -> Toolchain -> Symbols"
#endif  
//#define DISPLAYHW_F_CPU 4000000UL  

#if defined(F_CPU)
  #if (F_CPU != DISPLAYHW_F_CPU) 
    #warning "DISPLAYHW_F_CPU value in displayhw.h differs from F_CPU value" 
  #endif
#endif

// Внимание! Если меняете способ подключения дисплея, не забудьте в main.h поправить маски неиспользуемых пинов. Иначе возможны значительные токи утечки в режиме энергосбережения
// Note! If changing the display connection do not forget to change the unused pin masks in the main.h. Otherwise current leaks in power-save mode are possible



/* DISPLAYHW_MODE_I2C_TWI: Подключение по шине I2C, используя аппаратный интерфейс микроконтроллера TWI */
#define DISPLAYHW_MODE_I2C_TWI 

/* DISPLAYHW_MODE_I2C_SOFTWARE Подключение по шине I2C, используя программную реализацию на произвольных пинах */
//#define DISPLAYHW_MODE_I2C_SOFTWARE 

/* DISPLAYHW_MODE_SPI Подключение по шине SPI (4-х проводной вариант), используя аппаратный модуль SPI.
 * Вывод SS аппаратного SPI ОБЯЗАТЕЛЬНО должен быть настроен на выход. 
 * Если он настроен на вход и на нём появляется низкий уровень, то модуль SPI прервёт свою работу.
 * Рекомендуется этот вывод использовать для подключения линии CS# контроллера дисплея */
// #define DISPLAYHW_MODE_SPI


/* DISPLAYHW_MODE_SPI_USART Подключение по шине SPI, используя модуль USART микроконтроллера в режиме SPI-Master */
// #define DISPLAYHW_MODE_SPI_USART

/* DISPLAYHW_MODE_SPI_SOFTWARE Подключение по шине SPI (4-х проводной вариант), используя программную реализацию на произвольных пинах */
//#define DISPLAYHW_MODE_SPI_SOFTWARE


#if ((defined(DISPLAYHW_MODE_I2C_TWI) ? 1 : 0) \
   + (defined(DISPLAYHW_MODE_I2C_SOFTWARE) ? 1 : 0) \
   + (defined(DISPLAYHW_MODE_SPI) ? 1 : 0) \
   + (defined(DISPLAYHW_MODE_SPI_USART) ? 1 : 0) \
   + (defined(DISPLAYHW_MODE_SPI_SOFTWARE) ? 1 : 0) \
  ) != 1
  #error "One and only one display hardware mode should be selected" 
#endif

#if defined(DISPLAYHW_MODE_I2C_TWI) || defined(DISPLAYHW_MODE_I2C_SOFTWARE)

/**************************************************/
/* Программный и аппаратный I2C - общие настройки */
/**************************************************/

// Желаемая скорость работы шины I2C(TWI) 
// Desired baudrate for I2C(TWI) bus
#define I2C_BITRATE 400000UL 

// Адрес дисплея на шине I2C(TWI) 
// Display address on I2C bus
#define DISPLAY_I2C_ADDR 0x3C


#ifdef DISPLAYHW_MODE_I2C_SOFTWARE

/*******************/
/* Программный I2C */
/*******************/

// Буква порта, к пину которого подключена линия SCL
#define SOFTI2C_SCL_PORT_LETTER C

// Номер пина порта, к которому подключена линия SCL
#define SOFTI2C_SCL_PIN_NUM 5

// Буква порта, к пину которого подключена линия SDA
#define SOFTI2C_SDA_PORT_LETTER C

// Номер пина порта, к которому подключена линия SDA
#define SOFTI2C_SDA_PIN_NUM 4

#endif /* DISPLAYHW_MODE_I2C_SOFTWARE */ 


#endif /* defined(DISPLAYHW_MODE_TWI) || defined(DISPLAYHW_MODE_I2C_SOFTWARE) */

#if defined(DISPLAYHW_MODE_SPI_SOFTWARE) || defined(DISPLAYHW_MODE_SPI) || defined(DISPLAYHW_MODE_SPI_USART)

/***************************************************/
/* Программный и аппаратный SPI - общие настройки */
/**************************************************/


// Желаемая скорость работы шины SPI 
// Desired baudrate for SPI bus
#define SPI_BITRATE 10000000UL 


// Буква порта, к пину которого подключена линия D/C#
#define SPI_DC_PORT_LETTER D

// Номер пина порта, к которому подключена линия D/C#
#define SPI_DC_PIN_NUM 3



// При использовании аппаратного SPI модуля, линия CS# должна быть подключена к ножке SS микроконтроллера, либо SS должен быть сконфигурирован на вывод
// Если SS сконфигурирован на вход (соответствующий бит в регистре DDRx не установлен), то при появлении на нём низкого уровня работа модуля SPI будет остановлена

// Буква порта, к пину которого подключена линия CS# (SS)
#define SPI_CS_PORT_LETTER D

// Номер пина порта, к которому подключена линия CS# (SS)
#define SPI_CS_PIN_NUM 2


#ifdef DISPLAYHW_MODE_SPI 

/*******************/
/* Аппаратный SPI */
/*******************/


// Буква порта, на пинах которого работает модуль SPI
#define SPI_PORT_LETTER B

// Номер пина порта, на который выводится сигнал MOSI, к нему должна подключаться линия SDIN контроллера дисплея
#define SPI_MOSI_PIN_NUM 3

// Номер пина порта, на который выводится сигнал SCK, к нему должна подключаться линия CLK контроллера дисплея
#define SPI_SCK_PIN_NUM 5


#endif /* DISPLAYHW_MODE_SPI */


#ifdef DISPLAYHW_MODE_SPI_USART 

/****************************************************/
/* SPI посредством модуля USART в режиме SPI-Master */
/****************************************************/


/* Номер модуля USART, используемого для вывода данных на дисплей. Будут использоваться макросы с этим числом в названиях */
#define DISPLAY_USART_NUM 0

// Буква порта, на пинах которого работает модуль USART, назначенный дисплею
#define DISPLAY_USART_PORT_LETTER D

// Номер пина XCK модуля USART, назначенного дисплею
#define DISPLAY_USART_XCK_PIN_NUM 4

#endif /* DISPLAYHW_MODE_SPI_USART */


#ifdef DISPLAYHW_MODE_SPI_SOFTWARE

/*******************/
/* Программный SPI */
/*******************/


// Буква порта, к пину которого подключена линия SDIN (MOSI)
#define SOFTSPI_SDAT_PORT_LETTER B

// Номер пина порта, к которому подключена линия SDIN (MOSI)
#define SOFTSPI_SDAT_PIN_NUM 3


// Буква порта, к пину которого подключена линия SCLK (SCK)
#define SOFTSPI_SCLK_PORT_LETTER B

// Номер пина порта, к которому подключена линия SCLK (SCK)
#define SOFTSPI_SCLK_PIN_NUM 5


#endif /* DISPLAYHW_MODE_SPI_SOFTWARE */

#endif /* defined(DISPLAYHW_MODE_SPI_SOFTWARE) || defined(DISPLAYHW_MODE_SPI) || defined(DISPLAYHW_MODE_SPI_USART) */

/* Инициализирует аппаратную часть для связи с дисплеем
 * Возвращает не-ноль - в случае успеха, 0 - при ошибке
 * Initializes the hardware for communication to display
 * Returns: nonzero on success, zero on error */
uint8_t displayhw_init();

/* Немедленно освобождает всю используемую периферию и все используемые порты, позволяя микроконтроллеру перейти в режим энергосбережения
 * Frees all used peripherial and all used ports as fast as possible, allowing MCU to enter power-saving modes
 */
void displayhw_shutdown();


#endif /* DISPLAYHW */