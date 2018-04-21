// This file is generated automatically
// Language files max timestamp 21.4.2018 21:27:33
// Generated at 21.4.2018 21:27:42

#ifndef LOCALIZATION_H_
#define LOCALIZATION_H_

#include <avr/io.h>
#include <avr/pgmspace.h>

#define LANGUAGE_COUNT 2

#define LANGUAGE_EN 0
#define LANGUAGE_RU 1

#define LANGUAGE_DEFAULT LANGUAGE_RU

typedef struct {
  PGM_P en;
  PGM_P ru;
} LocalizedString;

extern PROGMEM LocalizedString const str_day_mo; // en: MONDAY; ru: ПОНЕД.
extern PROGMEM LocalizedString const str_day_tu; // en: TUESDY; ru: ВТОРН.
extern PROGMEM LocalizedString const str_day_we; // en: WEDNES; ru: СРЕДА
extern PROGMEM LocalizedString const str_day_th; // en: THURSD; ru: ЧЕТВ.
extern PROGMEM LocalizedString const str_day_fr; // en: FRIDAY; ru: ПЯТН.
extern PROGMEM LocalizedString const str_day_sa; // en: SATURD; ru: СУББ.
extern PROGMEM LocalizedString const str_day_su; // en: SUNDAY; ru: ВОСКР.
extern PROGMEM LocalizedString const str_menu_lang; // en: Language; ru: Язык / Language
extern PROGMEM LocalizedString const str_menu_set_time; // en: Set time; ru: Установка времени
extern PROGMEM LocalizedString const str_menu_set_date; // en: Set date; ru: Установка даты
extern PROGMEM LocalizedString const str_menu_calibrate; // en: Clock calibrate; ru: Калибровка хода
extern PROGMEM LocalizedString const str_menu_text_font; // en: Text font; ru: Шрифт текста
extern PROGMEM LocalizedString const str_menu_digits_font; // en: Digits font; ru: Стиль цифр
extern PROGMEM LocalizedString const str_menu_fx; // en: Visual effect; ru: Визуальный эффект
extern PROGMEM LocalizedString const str_menu_brightness_main; // en: Brightness (main); ru: Яркость (основн.)
extern PROGMEM LocalizedString const str_menu_brightness_alt; // en: Brightness (alt); ru: Яркость (альтер.)
extern PROGMEM LocalizedString const str_menu_temp; // en: Crystall t° cal.; ru: Калибр.t° кристалла
extern PROGMEM LocalizedString const str_menu_temp_koef; // en: Thermal koef.; ru: Темпер.коэфф.
extern PROGMEM LocalizedString const str_menu_fcpu; // en: CPU speed cal.; ru: Калибровка ЦП
extern PROGMEM LocalizedString const str_prompt_save; // en: Save changes?; ru: Сохранить?
extern PROGMEM LocalizedString const str_font_example; // en: ABC,xyz.0123!
extern PROGMEM LocalizedString const str_date_year; // en: Year; ru: Год
extern PROGMEM LocalizedString const str_date_month; // en: Month; ru: Месяц
extern PROGMEM LocalizedString const str_date_day; // en: Day; ru: День
extern PROGMEM LocalizedString const str_calib_ppm; // en: ppm; ru: ppm
extern PROGMEM LocalizedString const str_calib_spd; // en: sec/day; ru: сек/сутки
extern PROGMEM LocalizedString const str_temp_degree; // en: °C; ru: °C
extern PROGMEM LocalizedString const str_temp_koef_ppmpc; // en: ppm/°C²; ru: ppm/°C²
extern PROGMEM LocalizedString const str_fcpu_khz; // en: kHz; ru: кГц
extern PROGMEM LocalizedString const str_fcpu_calibration; // en: Calibration...; ru: Калибровка...
extern PROGMEM LocalizedString const str_info_firmware; // en: Firmware:; ru: Прошивка:
extern PROGMEM LocalizedString const str_info_firmware_date; // en: from; ru: от
extern PROGMEM LocalizedString const str_info_author; // en: Pogrebnyak Dmitry; ru: Погребняк Дмитрий
extern PROGMEM LocalizedString const str_info_author2; // en: Russia, Samara; ru: Россия, Самара
extern PROGMEM LocalizedString const str_language_name; // en: English; ru: Русский

// Возвращает код текущего выбранного языка
uint8_t get_language();

// Устанавливает заданный язык текущим
void set_language(uint8_t lang_code);

// В зависимости от выбранного текущего языка, возвращает указатель на строку из структуры локализации
PGM_P get_string(const LocalizedString * p_localized);

// В зависимости от указанного языка, возвращает указатель на строку из структуры локализации
PGM_P get_string_for(const LocalizedString * p_localized, uint8_t lang_code);

#endif /* LOCALIZATION_H_ */
