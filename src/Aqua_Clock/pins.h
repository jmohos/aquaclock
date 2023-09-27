/*
 * Pin definitions for the ESP32 microcontroller.
 * These are only for the ESP32 native pins.
 * See the io_expander_config.h for the IO expander pin definitions.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef PINS_H
#define PINS_H

// SPI pins used for the OLED display
#define SPI_OLED_MOSI_PIN 23
#define SPI_OLED_CLK_PIN 18
#define SPI_OLED_CS_PIN 14
#define SPI_OLED_DC_PIN 32
#define SPI_OLED_RST_PIN 15

#endif