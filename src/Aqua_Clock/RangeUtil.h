#pragma once
/*
 * Range Sensing Utility Class for the Aqua Clock
 *
 * Manages a single VL53L1X sensor with additional filtering and processing.
 * This requires the sensor to be connected to the microcontroller via its native I2C interface.
 * It handles the startup of the sensor and then periodically checks for new readings.
 * As new readings are found it will buffer the last three and provide the median of three in addition
 * to the raw readings.  A median reading is useful for filtering out a noisy single reading out of three.
 * If the sensor fails to initialize or does not provide a reading in a reasonable period then a system
 * fault is issued.
 * This has been testing using the Adafruit and Pololu VL53L1X carrier boards.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef RANGE_UTIL_H
#define RANGE_UTIL_H

#include <elapsedMillis.h>
#include <VL53L1X.h>
#include <Wire.h>

#include "faults.h"


class RangeUtil {

public:

  // Local error conditions for the sensor
  typedef enum {
    RANGE_SENSOR_UNINITIALIZED,
    RANGE_SENSOR_INIT_ERROR,
    RANGE_SENSOR_WORKING,
    RANGE_SENSOR_TIMEOUT
  } RANGE_SENSOR_STATE_T;


private:
  // Handle to the sensor driver
  VL53L1X* _device;

  // ID for the sensor to differentiate it from its peers
  uint8_t _sensor_num = -1;

  RANGE_SENSOR_STATE_T _sensor_state = RANGE_SENSOR_UNINITIALIZED;

  elapsedMillis _time_since_last_read_msec;
  const uint32_t READ_TIMEOUT_MSEC = 200;  // Max time between valid readings

  // History of raw readings
  static constexpr int HISTORY_SIZE = 3;
  uint16_t _range_history[HISTORY_SIZE];
  uint16_t _linearized_median_range = 0;
  uint8_t _median_index = 0;
  uint8_t _roi_center = 0;

public:

  RangeUtil(VL53L1X* sensor_device,
            uint8_t sensor_num) {
    _device = sensor_device;
    _sensor_num = sensor_num;
  }


  //
  // Setup the range sensor
  //
  bool Startup() {

    // VL53L1X sensor interface timeout to prevent blocking forever
    _device->setTimeout(500);

    // Try to initialize the sensor via I2C
    if (_device->init() == false) {
      // Sensor init has failed
      _sensor_state = RANGE_SENSOR_INIT_ERROR;
      Serial.print(F("Failed to boot VL53L1X for sensor: "));
      Serial.println(_sensor_num);

      // Set the appropriate system fault based on the device ID
      switch (_sensor_num) {
        case 1:
          FAULT_SET(FAULT_VL53L1X_SENSOR_1_INIT_FAIL);
          break;
        case 2:
          FAULT_SET(FAULT_VL53L1X_SENSOR_2_INIT_FAIL);
          break;
        case 3:
          FAULT_SET(FAULT_VL53L1X_SENSOR_3_INIT_FAIL);
          break;
        default:
          FAULT_SET(FAULT_VL53L1X_UNKNOWN_INIT_FAIL);
          break;
      }

      return false;
    }

    // Configure for a short range reading.
    _device->setDistanceMode(VL53L1X::Short);

    //_device->setMeasurementTimingBudget(50000);
    _device->setMeasurementTimingBudget(75000);
    //_device->setMeasurementTimingBudget(100000);

    // Set the Region of Interest window to the smallest, centered in the middle.
    // This creates the smallest field of view so we don't pick up stray readings off center.
    _device->setROISize(4, 4);

    // start continuous ranging internally with 25msec periodocity
    //_device->startContinuous(50);
    _device->startContinuous(25);

    // Init complete, ready for use
    _sensor_state = RANGE_SENSOR_WORKING;
    return true;
  }


  //
  // Range sensor master Loop
  //
  void Update() {
    uint16_t current_range;

    if (_sensor_state == RANGE_SENSOR_WORKING) {

      // Read the latest clock data from the RTC via I2C if it is ready
      if (_device->dataReady()) {
        current_range = process_reading(_device->read());

        _time_since_last_read_msec = 0;
      }

      // Detect device timeout
      if (_time_since_last_read_msec >= READ_TIMEOUT_MSEC) {
        // The range sensor failed to respond, disconnected?
        //Register fault based on device ID.
        switch (_sensor_num) {
          case 1:
            FAULT_SET(FAULT_VL53L1X_SENSOR_1_TIMEOUT);
            break;
          case 2:
            FAULT_SET(FAULT_VL53L1X_SENSOR_2_TIMEOUT);
            break;
          case 3:
            FAULT_SET(FAULT_VL53L1X_SENSOR_3_TIMEOUT);
            break;
          default:
            FAULT_SET(FAULT_VL53L1X_UNKNOWN_TIMEOUT);
            break;
        }
      }
    }
  }


  RANGE_SENSOR_STATE_T Get_State() {
    return _sensor_state;
  }


  // Report the most recent sensor value, unfiltered
  uint16_t Get_Newest_Reading() {
    return _range_history[0];
  }


  // Report the most recent median sensor value
  uint16_t Get_Median_Reading() {
    return _range_history[_median_index];
  }


  // Linearization is done externally and injected here
  void Set_Linearized_Median_Reading(uint16_t linearized_range) {
    _linearized_median_range = linearized_range;
  }

  // Report the most recent linearized median reading
  uint16_t Get_Linearized_Median_Reading() {
    return _linearized_median_range;
  }


protected:


  //
  // Filter the new raw reading with the prior readings.
  // Find the median value of the last three and return it.
  // This helps to reduce jitter in the readings.
  //
  uint16_t process_reading(uint16_t reading) {
    uint16_t max_range = 0;
    uint16_t max_range_index = 0;
    uint16_t min_range = 0xFFFF;
    uint16_t min_range_index = 0;

    _range_history[2] = _range_history[1];
    _range_history[1] = _range_history[0];
    _range_history[0] = reading;

    // Find highest and lowest
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (_range_history[i] > max_range) {
        max_range = _range_history[i];
        max_range_index = i;
      }
      if (_range_history[i] < min_range) {
        min_range = _range_history[i];
        min_range_index = i;
      }
    }

    // Find the median (not max or min of three samples)
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if ((i != min_range_index) && (i != max_range_index)) {
        _median_index = i;
        break;
      }
    }
    return _range_history[_median_index];
  }
};

#endif
