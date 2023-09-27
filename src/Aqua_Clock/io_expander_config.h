/*
 * IO Expander pin definitions
 *
 * A Sparkfun SX1509 IO expander board is connected to the ESP32 va I2C.
 * The expander board has 16 totals pins, configured for 8 inputs and 8 outputs.
 * The pin mappings are defined in this file.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef IO_EXPANDER_CONFIG_H
#define IO_EXPANDER_CONFIG_H


#include <Wire.h>            // Include the I2C library (required)
#include <SparkFunSX1509.h>  //Click here for the library: http://librarymanager/All#SparkFun_SX1509

// SX1509 I2C address (set by ADDR1 and ADDR0 (00 by default):
#define SX1509_ADDRESS 0x3E

// Pin Definitions Inputs
#define SX1509_PIN_WATER_LOW 0
#define SC1509_PIN_WATER_HIGH 1
#define SC1509_PIN_KEY_1 3
#define SC1509_PIN_KEY_2 4
#define SC1509_PIN_KEY_3 5
#define SC1509_PIN_KEY_4 6
#define SC1509_PIN_KEY_5 7


// Pin Definitions Outputs
#define SC1509_PIN_FEED_PUMP 8
#define SC1509_PIN_FEED_VALVE 12
#define SC1509_PIN_HOUR_FEED 15
#define SC1509_PIN_HOUR_DRAIN 11
#define SC1509_PIN_MIN_10s_FEED 14
#define SC1509_PIN_MIN_10s_DRAIN 10
#define SC1509_PIN_MIN_1s_FEED 13
#define SC1509_PIN_MIN_1s_DRAIN 9

#endif
