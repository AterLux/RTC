/*
 * display.h
 *
 * Модуль работы с монохромными дисплеями
 *
 * Author: Погребняк Дмитрий (Pogrebnyak Dmitry, http://aterlux.ru/)
 */ 


#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <avr/io.h>


/* Используемый драйвер дисплея. */

// Заголовочный файл драйвера дисплея должен описывать (макро-определениями) следующие константы:
// The header file of the display driver should define next constants (as #define macros)
// DISPLAY_WIDTH - ширина дисплея в пикселях / width of the display in pixels
// DISPLAY_PAGES - высота рабочей части дисплея в страницах (8-пиксельных блоках)  / height of the display in pages (8-pixel blocks)
// DISPLAY_HEIGHT - высота видимой области дисплея в пикселях (обычно DISPLAY_PAGES * 8) / height of the visible display area in pixels (usually DISPLAY_PAGES * 8)
// Также, если поддерживается управление контрастностью, то DISPLAY_CONTRAST_MAX_VAL определяет максимальное значение, принимаемое на вход функции display_set_contrast()
// Also if constrast control is supported, then DISPLAY_CONTRAST_MAX_VAL defines the maximal value of argument to function display_set_contrast()


// SSD1306 используется в монохронмных OLED дисплеях. Типичные в виде модулей 128х32 (0.91") и 128x64 (0.96", 1.3")
#include "ssd1306.h"






#include "../pulling.h"

// Макрос safe_delay_ms определяет метод, которым выполняется задержка на указанное число миллисекунд. 
// Результатом работы макроса должно быть не нулевое число, если задержка исполнена, и 0 - если необходимо прекратить операции
// Например, можно переопределить этот макрос для выполнения периодических операций
// Значение по умолчанию ({_delay_ms(ms); 1; })
#define safe_delay_ms(ms) ({pause_ms(ms); 1; })

// Макрос check_abort выполняет проверку необходимости прервать работу. Возвращает не нулевое значение, если необходимо прервать работу с дисплеем
// Вызывается в функциях ожидания. По умолчанию принимает значение константы 0
#define check_abort() (0)




/***** АППАРАТНО-ЗАВИСИМЫЕ ФУНКЦИИ *****/
// функции, описанные ниже должны быть реализованы конкретным драйвером дисплея


/* Инициализирует дисплей
 * 
 * Initializes the display
 */
uint8_t display_init();



/* Начинает передачу графических данных на дисплей.
 * Графические данные кодируют столбец из 8 пикселей изображения. Младший бит кодирует верхний пиксель.
 * После вывода данных курсор перемещается на 1 позицию вправо так, что следующий вызов этой функции выведет данные на 1 позицию правее
 * Поведение функции при выходе за пределы экрана - не определено
 * Возвращает не-ноль - в случае успеха, 0 - при ошибке
 * Inits transmission of a graphical data byte
 * The data byte encodes column of 8 pixels of the image, the least significant bit encodes topmost pixel.
 * After the data is out, the cursor is shifted one position right, so next call to this function will output a column of 8th pixels one position to the right
 * When cursor is out of the rightmost position on the screen, behavior of the function is undefinite.
 * Returns: nonzero on success, zero on error */
uint8_t display_data(uint8_t d);

/* Начинает передачу команды контроллеру дисплея 
 * Возвращает не-ноль - в случае успеха, 0 - при ошибке
 * Inits transmission of the command to the display controller 
 * Returns: nonzero on success, zero on error */
uint8_t display_cmd(uint8_t c);

/* Начинает передачу 2 байт команд контроллеру дисплея
 * Возвращает не-ноль - в случае успеха, 0 - при ошибке
 * Inits transmission of two command bytes to the display controller 
 * Returns: nonzero on success, zero on error */
uint8_t display_cmd2(uint8_t c1, uint8_t c2);

/* Дожидается завершение всех асинхронных операций по передаче данных дисплею и завершает сеанс обмена.
 * Возвращает не-ноль - в случае успеха, 0 - при ошибке
 * Inits transmission of two commands to the display controller 
 * Waits for asynchronous transmission operations to complete and finalizes the communication session 
 * Returns: nonzero on success, zero on error */
uint8_t display_end();



/* Готовит контроллер дисплея и затем переводит его в режим экономии энергии.
 * Для повторной инициализации дисплея необходимо вызвать  display_init() вновь
 * Команда завершается синхронно, т.е. не должно требоваться display_end() после её завершения
 * 
 * Preapares the display controller, and put it to power-down mode.
 * To resume from power-down, call display_init() again
 * This command performs synchroniously, i.e. it should not require to call display_end() after
 */
uint8_t display_powerdown();

/* Возобновляет работу дисплея, переведённого в режим экономии энергии командой display_powerdown()
 * Команда завершается синхронно, т.е. не должно требоваться display_end() после её завершения
 * Resumes display operation after it was put in sleep mode by display_powerdown()
 * This command performs synchroniously, i.e. it should not require to call display_end() after
 */
uint8_t display_resume_power();

#ifdef DISPLAY_CONTRAST_MAX_VAL
/* Устанавливает контрастность (яркость) дисплея. Значение в дипазоне 0-DISPLAY_CONTRAST_MAX_VAL
 * Команда завершается синхронно, т.е. не должно требоваться display_end() после её завершения
 * Sets the brigthness (contrast) of the display. Value in the range 0-DISPLAY_CONTRAST_MAX_VAL
 * This command performs synchroniously, i.e. it should not require to call display_end() after
 */
uint8_t display_set_contrast(uint8_t x);
#endif


/* Выбирает страницу (позицию по вертикали) для вывода 
 * page - страница от 0 до DISPLAY_PAGES - 1
 * 
 * Selects page (vertical postion) from 0 to  DISPLAY_PAGES - 1 */
uint8_t display_setpage(uint8_t page);

/* Выбирает горизонтальную позицию для вывода
 * x - позиция от 0 до DISPLAY_WIDTH - 1
 * 
 * Selects horizontal position from 0 to DISPLAY_WIDTH - 1 */
uint8_t display_setx(uint8_t x);

/* Выбирает страницу и горизонтальную позицию для вывода
 * page - страница от 0 до  DISPLAY_PAGES - 1
 * x - позиция от 0 до DISPLAY_WIDTH - 1
 * 
 * Selects both page and horizontal position at once
 * */
uint8_t display_setpos(uint8_t page, uint8_t x);



/***** УТИЛИТЫ *****/
// функции, описанные ниже, реализованы в display.c и одинаковы для всех

/* Заполняет указанную область заданными чередующимися значениями 
 * Fills the screen area with interleaved values */
uint8_t display_fill_region(uint8_t page, uint8_t x, uint8_t pages_height, uint8_t width, uint8_t evenData, uint8_t oddData);

/* Заполняет экран заданными чередующимися значениями 
 * Fills the screen with interleaved values */
uint8_t display_fill(uint8_t evenData, uint8_t oddData);

/* Очищает экран, устанавливает курсор в левый верхний угол 
 * Clears the screen. Moves cursor to the left upper corner */
uint8_t display_clear();

/* Выводит на экран изображение из флеш-памяти
 * page, x - начальная позиция для вывода изображения
 * pData - указатель на изображение во флеш-памати
 * pages_height - высота изоюражения (в страницах)
 * width - ширина изображения
 *
 * Transmits bytes from the flash memory to the display region
 * page, x - initial position for the image output
 * pData - pointer to the image at the flash memory
 * pages_height - height of the image in pages
 * width - width of the image
 */
uint8_t display_sprite(uint8_t page, uint8_t x, PGM_VOID_P pData, uint8_t pages_height, uint8_t width);

/* Копирует из буфера изображение на экран
 * page, x - начальная позиция для вывода буфера
 * pData - указатель на буфер
 * pages_height - высота буфера (в страницах)
 * width - ширина буфера
 * 
 * Transmits image from the offscreen buffer to the display region.
 * page, x - initial position for the buffer output
 * pData - pointer to the buffer
 * pages_height - height of the buffer in pages
 * width - width of the buffer area
 */
uint8_t display_buffer(uint8_t page, uint8_t x, uint8_t * pData, uint8_t pages_height, uint8_t width);



#endif /* DISPLAY_H_ */