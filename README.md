# RTC
AVR Real Time Clock

This is a realization of Real Time Clock with battery back-up on the AVR family ATmega328-P MCU.

This is how it works: https://www.youtube.com/watch?v=TeBulb6ntlE

The project uses 128x32 monochrome OLED display based on SSD1306 controller, connected by I2C bus.

![Schematics](https://raw.githubusercontent.com/AterLux/RTC/master/rtc_schematics.gif)

### Fuses
Low: 0x62;
High: 0xDE;
Extended: 0xFE

### Bootloader

https://raw.githubusercontent.com/AterLux/RTC/master/RTCBootloader/Debug/RTCBootloader.hex

### Setup tool

This tool (for Windows only) can be used to upload main firmware and sych time: https://aterlux.ru/files/RTCsetup.zip

### Firmware

https://raw.githubusercontent.com/AterLux/RTC/master/RTC/Debug/RTC.hex
