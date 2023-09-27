/*
 * Reservoir Tank Manager class for the Aqua Clock
 *
 * Manages the regulation of a water transfer from the drain tank to the feed tank.
 * The upper tank has two water level sensors set at a low (25%) and high (75%) water mark.
 * If the water level of this tank falls below the lower water mark then a pump is turned on to fill
 * the tank up to the high level sensor.
 * There is support for manual actuation of the pump for service modes to fill the tanks.
 * There is fault monitoring applied to look for:
 *   Excessive pump time to fill the tank.  (Lower tank probably running dry, leak or bad pump)
 *   Invalid water level sensor readings.  (Failed sensor, miswired sensors)
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef TANK_MANAGER_H
#define TANK_MANAGER_H

#include <Arduino.h>
#include <elapsedMillis.h>

#include "io_expander_config.h"


class TankManager {

public:

  typedef enum {
    TANK_IDLE,
    TANK_FILL_ACTIVE,
    TANK_FILL_SETTLE,
    TANK_MANUAL_FILL,
    TANK_FILL_TIMEOUT_FAULT
  } TANK_STATE_TYPE_T;

private:

  /* All discrete IO handled by IO Expander board */
  SX1509 *_io_expander;
  uint8_t _feed_pump_drive_pin = -1;
  uint8_t _feed_tank_level_high_pin = -1;
  uint8_t _feed_tank_level_low_pin = -1;
  bool _feed_tank_level_above_high = false;
  bool _feed_tank_level_above_low = false;
  bool _pump_active = false;

  bool _enable = false;
  bool _enable_logging = false;

  TANK_STATE_TYPE_T _state = TANK_IDLE;

  static constexpr int TANK_UPDATE_PERIOD_MSEC = 10;  // 100Hz
  elapsedMillis _time_in_current_state;
  elapsedMillis _time_since_last_update;

  bool _request_manual_fill = false;
  uint32_t _manual_fill_period = 2000;
  uint32_t _post_pump_settle_period = 1000;

  const uint32_t MAX_PUMP_FILL_TIME_MSEC = 30000; /* 30 second pump time max */

public:

  //
  // Constructor - Capture handles to IO expander and the relevant pins.
  // IO expander and pin definitions must already have been configured prior to this point.
  //
  TankManager(SX1509 *io_expander,
              uint8_t feed_pump_pin,
              uint8_t feed_tank_level_sense_low_pin,
              uint8_t feed_tank_level_sense_high_pin) {
    _io_expander = io_expander;
    _feed_pump_drive_pin = feed_pump_pin;
    _feed_tank_level_low_pin = feed_tank_level_sense_low_pin;
    _feed_tank_level_high_pin = feed_tank_level_sense_high_pin;

    stop_pumping();
  }


  TANK_STATE_TYPE_T Get_State() {
    return (_state);
  }


  bool Is_Feed_Tank_Above_High_Mark() {
    return _feed_tank_level_above_high;
  }


  bool Is_Feed_Tank_Above_Low_Mark() {
    return _feed_tank_level_above_low;
  }


  bool Is_Tank_Regulator_Enabled() {
    return _enable;
  }


  void Set_Regulator_Enable(bool enable) {
    _enable = enable;
  }


  bool Manual_Fill(uint32_t period) {
    _manual_fill_period = period;
    _request_manual_fill = true;
    return false;
  }


  bool Is_Pump_Active() {
    return _pump_active;
  }


  void Enable_Logging() {
    _enable_logging = true;
  }


  void Disable_Logging() {
    _enable_logging = false;
  }


  //
  // Periodic update of the tank manager.
  // Reads the tank level sensors and then decides if the pump should run or not.
  // Also performs fault monitoring to catch relevant faults.
  //
  bool Update() {

    update_feed_tank_level_status();

    // Detect invalid tank level sensor readings
    if ((Is_Feed_Tank_Above_Low_Mark() == false) && (Is_Feed_Tank_Above_High_Mark() == true)) {
      // The high water level cannot be active without the low water level also being active.
      // This indicates a sensing failure.  Either the sensors are swapped or one of the sensors
      // is not working. Register the failure.
      FAULT_SET(FAULT_TANK_LEVEL_SENSE_FAIL);
    }

    // Rate limit the processing
    if (_time_since_last_update < TANK_UPDATE_PERIOD_MSEC) {
      return true;
    }
    _time_since_last_update = 0;

    //
    // Update the state machine to manage regulation sequencing
    //
    switch (_state) {
      case TANK_IDLE:
        stop_pumping();

        if (_request_manual_fill) {
          _request_manual_fill = false;
          if (_enable_logging) {
            Serial.print("TANK: Starting tank manual fill: ");
            Serial.println(_manual_fill_period);
          }
          _state = TANK_MANUAL_FILL;
          _time_in_current_state = 0;
        } else if (_enable) {
          if (!Is_Feed_Tank_Above_Low_Mark()) {
            // Running low on water, begin process of filling
            _state = TANK_FILL_ACTIVE;
            _time_in_current_state = 0;

            if (_enable_logging) {
              Serial.println("TANK: IDLE to FILL_ACTIVE");
            }
          }
        }
        break;

      case TANK_FILL_ACTIVE:
        if (!_enable) {
          _state = TANK_IDLE;
          _time_in_current_state = 0;
        }

        start_pumping();

        if (Is_Feed_Tank_Above_High_Mark()) {
          stop_pumping();
          _state = TANK_FILL_SETTLE;
          _time_in_current_state = 0;

          if (_enable_logging) {
            Serial.println("TANK: FILL_ACTIVE to FILL_SETTLE");
          }
        }

        if (_time_in_current_state >= MAX_PUMP_FILL_TIME_MSEC) {
          // Took too long to fill tank, something is wrong, register fault
          FAULT_SET(FAULT_TANK_FILL_TIMEOUT);
        }
        break;

      case TANK_FILL_SETTLE:
        if (!_enable) {
          _state = TANK_IDLE;
          _time_in_current_state = 0;
        }

        stop_pumping();

        if (_time_in_current_state >= _post_pump_settle_period) {
          _state = TANK_IDLE;
          _time_in_current_state = 0;

          if (_enable_logging) {
            Serial.println("TANK: FILL_SETTLE to TANK_IDLE");
          }
        }
        break;

      case TANK_MANUAL_FILL:
        start_pumping();

        if (_time_in_current_state >= _manual_fill_period) {
          _state = TANK_IDLE;
          _time_in_current_state = 0;
        }
        break;

      case TANK_FILL_TIMEOUT_FAULT:
        // Stay here till the fault is manually cleared or system restarted
        break;

      default:
        // Should never get here
        stop_pumping();
        break;
    }

    return false;
  }

private:

  void update_feed_tank_level_status() {
    if (_feed_tank_level_high_pin >= 0) {
      _feed_tank_level_above_high = !_io_expander->digitalRead(_feed_tank_level_high_pin);
    }
    if (_feed_tank_level_low_pin >= 0) {
      _feed_tank_level_above_low = !_io_expander->digitalRead(_feed_tank_level_low_pin);
    }
  }


  void start_pumping() {
    if (_feed_pump_drive_pin >= 0) {
      // Using digital output mode:
      _io_expander->digitalWrite(_feed_pump_drive_pin, HIGH);

      _pump_active = true;
    }
  }


  void stop_pumping() {
    if (_feed_pump_drive_pin >= 0) {
      // Using digital output mode:
      _io_expander->digitalWrite(_feed_pump_drive_pin, LOW);

      _pump_active = false;
    }
  }
};

#endif
