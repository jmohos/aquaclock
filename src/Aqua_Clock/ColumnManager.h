/*
 * Column Manager Class for the Aqua Clock
 *
 * Manages the regulation of a single column of water by actuating feed and drain valves.
 * The control input is a range reading from a Time of Flight sensor located above the column.
 * A state machine is used to track the current column control state.  
 * There is support for manual valve actuation used for filling, draining or calbrating the column.
 * Built in diagnostics monitor the regulation to look for unusual situations.  If a fault condition is
 * detected the column regulator is disabled and a fault is registered with the system.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef COLUMN_MANAGER_H
#define COLUMN_MANAGER_H

#include <Arduino.h>
#include <elapsedMillis.h>

#include "io_expander_config.h"


class ColumnManager {
public:
  typedef enum {
    COLUMN_IDLE,
    COLUMN_DRAIN_ACTIVE,
    COLUMN_DRAIN_SETTLE,
    COLUMN_FILL_ACTIVE,
    COLUMN_FILL_SETTLE,
    COLUMN_MANUAL_DRAIN,
    COLUMN_MANUAL_FILL,
    COLUMN_ERROR_STATE
  } COLUMN_STATE_TYPE_T;

  typedef enum {
    CONTROL_ERROR_DEADBAND,
    CONTROL_ERROR_POSITIVE,
    CONTROL_ERROR_NEGATIVE
  } CONTROL_ERROR_STATE_TYPE_T;


private:
  // IO Expander and pins to actuate
  SX1509 *_io_expander;
  uint8_t _feed_valve_actuator_pin = -1;
  uint8_t _drain_valve_actuator_pin = -1;

  // ID to keep track of which clock column this object manages
  uint8_t _column_num = -1;

  // Commanded setpoint
  uint16_t _setpoint_mm = 150;

  // Elevation feedback limits and current value
  uint16_t _elevation_lower_limit = 50;
  uint16_t _elevation_upper_limit = 305;
  uint16_t _elevation_mm = 150;
  uint16_t _setpoint_deadband = 4; /* At setpoint if within +/- this value */

  // Enables for operations
  bool _regulator_enable = false;
  bool _logging_enable = false;

  COLUMN_STATE_TYPE_T _state = COLUMN_IDLE;

  // Timing monitoring
  elapsedMillis _time_in_current_state;
  uint32_t _manual_drain_period = 2000;
  uint32_t _manual_fill_period = 2000;
  uint32_t _drain_dwell_period = 1000;
  uint32_t _fill_dwell_period = 1000;
  uint32_t _max_column_drain_period = 60000;
  uint32_t _max_column_fill_period = 60000;

  bool _request_manual_fill = false;
  bool _request_manual_drain = false;

  // Result of controller comparison of setpoint and process variable
  CONTROL_ERROR_STATE_TYPE_T _control_error_state = CONTROL_ERROR_DEADBAND;


public:
  /* Constructor - capture resources and parameters linked to this object */
  ColumnManager(uint8_t column_num,
                SX1509 *io_expander,
                uint8_t feed_pin,
                uint8_t drain_pin,
                uint16_t elevation_lower_limit,
                uint16_t elevation_upper_limit) {
    _column_num = column_num;
    _io_expander = io_expander;
    _feed_valve_actuator_pin = feed_pin;
    _drain_valve_actuator_pin = drain_pin;
    _elevation_lower_limit = elevation_lower_limit;
    _elevation_upper_limit = elevation_upper_limit;

    // Pin modes have already been set at this point
    stop_flows();
  }


  COLUMN_STATE_TYPE_T Get_State() {
    return (_state);
  }

  CONTROL_ERROR_STATE_TYPE_T Get_Control_Error_State() {
    return (_control_error_state);
  }

  void Set_Regulator_Enable(bool enable) {
    _regulator_enable = enable;
  }

  bool Is_Column_Regulator_Enabled() {
    return _regulator_enable;
  }

  uint16_t Get_Target_Setpoint_MM() {
    return _setpoint_mm;
  }

  uint16_t Get_Setpoint_Lower_Limit() {
    return _elevation_lower_limit;
  }

  uint16_t Get_Setpoint_Upper_Limit() {
    return _elevation_upper_limit;
  }

  bool Set_Elevation_Reading_MM(uint16_t elevation_mm) {
    _elevation_mm = elevation_mm;
    return false;
  }

  uint16_t Get_Elevation_Reading_MM() {
    return _elevation_mm;
  }

  bool Manual_Drain(uint32_t period) {
    _manual_drain_period = period;
    _request_manual_drain = true;
    return false;
  }

  bool Manual_Fill(uint32_t period) {
    _manual_fill_period = period;
    _request_manual_fill = true;
    return false;
  }

  void Enable_Logging() {
    _logging_enable = true;
  }

  void Disable_Logging() {
    _logging_enable = false;
  }


  //
  // Update the valve regulation periodically.
  // Returns true if it is busy adjusting the water level so we can hold off running the pump.
  //
  bool Update(uint16_t current_elevation_mm,
              uint16_t setpoint_mm) {
    //              bool fill_override,
    //              bool drain_override) {
    bool busy = false;

    _elevation_mm = current_elevation_mm;
    _setpoint_mm = setpoint_mm;

    if (_setpoint_mm < _elevation_lower_limit) {
      _setpoint_mm = _elevation_lower_limit;
    }
    if (_setpoint_mm > _elevation_upper_limit) {
      _setpoint_mm = _elevation_upper_limit;
    }


    // Look for requests to manually actuate the valves.  These are higher priority than the regulator
    // functions because they are for service actions.
    if (_request_manual_fill) {
      _request_manual_fill = false;
      _state = COLUMN_MANUAL_FILL;
      _time_in_current_state = 0;

      if (_logging_enable) {
        Serial.print("COLUMN ");
        Serial.print(_column_num);
        Serial.println(": current state to COLUMN_MANUAL_FILL");
      }
    }
    if (_request_manual_drain) {
      _request_manual_drain = false;
      _state = COLUMN_MANUAL_DRAIN;
      _time_in_current_state = 0;

      if (_logging_enable) {
        Serial.print("COLUMN ");
        Serial.print(_column_num);
        Serial.println(": current state to COLUMN_MANUAL_DRAIN");
      }
    }

    // Update the control error
    _control_error_state = control_loop_update();

    /* Run a state machine to manage regulation */
    switch (_state) {
      case COLUMN_IDLE:
        stop_flows();

        if (_regulator_enable) {
          // Determine how to respond to control error state if we are out of deadband
          if (_control_error_state == CONTROL_ERROR_POSITIVE) {
            // Need to fill up the column to raise the level
            _state = COLUMN_FILL_ACTIVE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_IDLE to COLUMN_FILL_ACTIVE");
            }
          } else if (_control_error_state == CONTROL_ERROR_NEGATIVE) {
            // Need to drain the column to lower the level
            _state = COLUMN_DRAIN_ACTIVE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_IDLE to COLUMN_DRAIN_ACTIVE");
            }
          }
        }
        break;

      case COLUMN_DRAIN_ACTIVE:
        if (!_regulator_enable) {
          _state = COLUMN_DRAIN_SETTLE;
          _time_in_current_state = 0;
        } else {

          // Are we still in the negative error range or have we hit deadband?
          if (_control_error_state != CONTROL_ERROR_NEGATIVE) {
            stop_flows();
            _state = COLUMN_DRAIN_SETTLE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_DRAIN_ACTIVE to COLUMN_DRAIN_SETTLE");
            }
          } else if (_time_in_current_state >= _max_column_drain_period) {
            stop_flows();
            Serial.println("ERROR: Spent too long in COLUMN_DRAIN_ACTIVE state!");
            _state = COLUMN_ERROR_STATE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_DRAIN_ACTIVE to COLUMN_ERROR_STATE");
            }
          } else {
            // Open valve to drain column and drop elevation
            start_draining();
            busy = true;
          }
        }
        break;

      case COLUMN_DRAIN_SETTLE:
        stop_flows();
        if (_time_in_current_state >= _drain_dwell_period) {
          _state = COLUMN_IDLE;
          _time_in_current_state = 0;

          if (_logging_enable) {
            Serial.print("COLUMN ");
            Serial.print(_column_num);
            Serial.println(": COLUMN_DRAIN_SETTLE to COLUMN_IDLE");
          }
        }
        break;

      case COLUMN_FILL_ACTIVE:
        if (!_regulator_enable) {
          _state = COLUMN_FILL_SETTLE;
          _time_in_current_state = 0;
        } else {

          // Are we still in the positive error range or have we hit deadband?
          if (_control_error_state != CONTROL_ERROR_POSITIVE) {
            stop_flows();
            _state = COLUMN_FILL_SETTLE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_FILL_ACTIVE to COLUMN_FILL_SETTLE");
            }
          } else if (_time_in_current_state >= _max_column_fill_period) {
            stop_flows();
            Serial.println("ERROR: Spent too long in COLUMN_FILL_ACTIVE state!");
            _state = COLUMN_ERROR_STATE;
            _time_in_current_state = 0;

            if (_logging_enable) {
              Serial.print("COLUMN ");
              Serial.print(_column_num);
              Serial.println(": COLUMN_FILL_ACTIVE to COLUMN_ERROR_SETTLE");
            }
          } else {
            // Open valve to fill column and raise elevation
            start_filling();
            busy = true;
          }
        }
        break;

      case COLUMN_FILL_SETTLE:
        stop_flows();
        if (_time_in_current_state >= _fill_dwell_period) {
          _state = COLUMN_IDLE;
          _time_in_current_state = 0;

          if (_logging_enable) {
            Serial.print("COLUMN ");
            Serial.print(_column_num);
            Serial.println(": COLUMN_FILL_SETTLE to COLUMN_IDLE");
          }
        }
        break;

      case COLUMN_MANUAL_DRAIN:
        start_draining();
        busy = true;
        if (_time_in_current_state >= _manual_drain_period) {
          _state = COLUMN_IDLE;

          if (_logging_enable) {
            Serial.print("COLUMN ");
            Serial.print(_column_num);
            Serial.println(": COLUMN_MANUAL_DRAIN to COLUMN_IDLE");
          }
        }
        break;

      case COLUMN_MANUAL_FILL:
        start_filling();
        busy = true;
        if (_time_in_current_state >= _manual_fill_period) {
          _state = COLUMN_IDLE;

          if (_logging_enable) {
            Serial.print("COLUMN ");
            Serial.print(_column_num);
            Serial.println(": COLUMN_MANUAL_FILL to COLUMN_IDLE");
          }
        }
        break;

      case COLUMN_ERROR_STATE:
        stop_flows();
        break;

      default:
        break;
    }

    return busy;
  }

protected:
  //
  // Evaluate if the control loop is outside or inside the control loop deadband.
  //
  CONTROL_ERROR_STATE_TYPE_T control_loop_update() {

    int16_t delta = _elevation_mm - _setpoint_mm;

    // Are we in a deadband around the setpoint?
    if (abs(delta) <= _setpoint_deadband) {
      return CONTROL_ERROR_DEADBAND;
    } else if (delta > 0) {
      return CONTROL_ERROR_POSITIVE;
    } else {
      return CONTROL_ERROR_NEGATIVE;
    }
  }


  //
  // Adjust valves to allow water to flow from the top tank into column to raise the level.
  //
  void start_filling() {
    if (_feed_valve_actuator_pin >= 0) {
      _io_expander->digitalWrite(_feed_valve_actuator_pin, HIGH);
    }
    if (_drain_valve_actuator_pin >= 0) {
      _io_expander->digitalWrite(_drain_valve_actuator_pin, LOW);
    }
  }


  //
  // Adjust the valves to allow water to flow from the column to the drain tank to lower the level.
  //
  void start_draining() {
    if (!FAULT_ACTIVE(FAULT_SX1509_IO_EXPANDER_INIT_FAIL)) {
      if (_feed_valve_actuator_pin >= 0) {
        _io_expander->digitalWrite(_feed_valve_actuator_pin, LOW);
      }
      if (_drain_valve_actuator_pin >= 0) {
        _io_expander->digitalWrite(_drain_valve_actuator_pin, HIGH);
      }
    }
  }


  //
  // Close all valves and keep the water level constant
  //
  void stop_flows() {
    if (!FAULT_ACTIVE(FAULT_SX1509_IO_EXPANDER_INIT_FAIL)) {
      if (_feed_valve_actuator_pin >= 0) {
        _io_expander->digitalWrite(_feed_valve_actuator_pin, LOW);
      }
      if (_drain_valve_actuator_pin >= 0) {
        _io_expander->digitalWrite(_drain_valve_actuator_pin, LOW);
      }
    }
  }
};

#endif
