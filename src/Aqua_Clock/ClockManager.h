/*
 * Clock management class for the Aqua Clock
 *
 * Manages the reading and setting of time for use by the rest of the system.
 * An I2C based offboard RV-8803 board from Sparkfun is used for battery backed RTC.
 * It also supports reporting if the clock is in a "sleep" windows.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include <Arduino.h>
#include <elapsedMillis.h>

#include <SparkFun_RV8803.h>  //Get the library here:http://librarymanager/All#SparkFun_RV-8803

#include "faults.h"


class ClockManager {
public:

private:
  typedef enum {
    CLOCK_MANAGER_UNINITIALIZED,
    CLOCK_MANAGER_INIT_ERROR,
    CLOCK_MANAGER_WORKING,
    CLOCK_MANAGER_TIMEOUT
  } CLOCK_MANAGER_STATE_T;

  CLOCK_MANAGER_STATE_T _clock_manager_state = CLOCK_MANAGER_UNINITIALIZED;

  static constexpr int RTC_UPDATE_PERIOD_MSEC = 1000;  // 1Hz

  RV8803 _rtc_offboard;  // Offboard RV8803 RTC handle

  elapsedMillis _time_since_last_update = RTC_UPDATE_PERIOD_MSEC + 1;

  bool _online = false;

  // Retrieved parameters from external RTC
  uint8_t _rtc_seconds;
  uint8_t _rtc_minutes;
  uint8_t _rtc_hours;
  uint8_t _rtc_date;
  uint8_t _rtc_weekday;
  uint8_t _rtc_month;
  uint16_t _rtc_year;

  // Timeframe for operating in Wake vs Sleep periods.
  // Set defaults of 7am to 7pm.  Values are restored/adjusted in the UI.
  uint8_t _wake_hour = 7;
  uint8_t _wake_min = 0;
  ;
  uint8_t _sleep_hour = 19;
  uint8_t _sleep_min = 0;

  bool _in_sleep = false;


public:
  /* Constructor */
  ClockManager() {
  }


  //
  // Setup the clock manager
  //
  bool Startup() {

    // Wire interface must be active prior to using this code.
    // Ensure that Wire.begin() has already been called.

    // Find the offboard RTC
    if (_rtc_offboard.begin() == false) {
      // Set system fault due to failed access to critical clock
      FAULT_SET(FAULT_RV8803_RTC_INIT_FAIL);
      _clock_manager_state = CLOCK_MANAGER_INIT_ERROR;
      Serial.println("ERROR: offboard RV-8803 RTC device init fault!");
      return false;
    }

    // Operate the RTC in 24 hour mode, use GMT/UTC (no time zone offset).
    _rtc_offboard.set24Hour();
    _rtc_offboard.setTimeZoneQuarterHours(0);
    Serial.println("RV-8803 offboard RTC online!");

    // Initialization done, ready for use
    _clock_manager_state = CLOCK_MANAGER_WORKING;
    return true;
  }


  //
  // Master Loop
  //
  void Update() {
    if (_time_since_last_update >= RTC_UPDATE_PERIOD_MSEC) {
      _time_since_last_update = 0;

      if (_clock_manager_state == CLOCK_MANAGER_WORKING) {
        if (_rtc_offboard.updateTime() == false) {
          FAULT_SET(FAULT_RV8803_RTC_READ_FAULT);
          Serial.println("ERROR: Failed to read offboard RTC time!");
          _clock_manager_state = CLOCK_MANAGER_TIMEOUT;
        } else {
          // Capture the fresh RTC readings
          _rtc_seconds = _rtc_offboard.getSeconds();
          _rtc_minutes = _rtc_offboard.getMinutes();
          _rtc_hours = _rtc_offboard.getHours();
          _rtc_date = _rtc_offboard.getDate();
          _rtc_weekday = _rtc_offboard.getWeekday();
          _rtc_month = _rtc_offboard.getMonth();
          _rtc_year = _rtc_offboard.getYear();

          // Determine if we are in a sleep window or not
          _in_sleep = detect_sleep_window();
        }
      }
    }
  }


  bool Is_Working() {
    return (_clock_manager_state == CLOCK_MANAGER_WORKING);
  }


  bool Is_Sleep_Time() {
    return _in_sleep;
  }


  uint16_t Get_Second() {
    return _rtc_seconds;
  }


  uint16_t Get_Minute() {
    return _rtc_minutes;
  }


  uint16_t Get_Hour() {
    return _rtc_hours;
  }


  uint16_t Get_Day() {
    return _rtc_date;
  }


  // Day of week, 0=sunday and 6=saturday
  uint16_t Get_Day_of_Week() {
    return _rtc_weekday;
  }


  uint16_t Get_Month() {
    return _rtc_month;
  }


  uint16_t Get_Year() {
    return _rtc_year;
  }


  //
  // Set time and date using discrete values for all time fields.
  //
  bool Set_Time(uint8_t sec, uint8_t min, uint8_t hour, uint8_t weekday, uint8_t date, uint8_t month, uint16_t year) {
    if (_clock_manager_state == CLOCK_MANAGER_WORKING) {
      if (_rtc_offboard.setTime(sec, min, hour, weekday, date, month, year) == false) {
        // Failed to set time.
        FAULT_SET(FAULT_RV8803_RTC_SET_TIME_FAULT);
        _clock_manager_state = CLOCK_MANAGER_TIMEOUT;
        Serial.println("ERROR: Failed to set RTC time!");
        return false;
      } else {
        // Time set successfully
        return true;
      }
    } else {
      // Clock manager is offline
      return false;
    }
  }


  //
  // Set all time values with a single epoch number.  Seconds since Jan 1, 1970.
  // Must adjust for local time zone.
  //
  bool Set_Time_Epoch(unsigned long epoch) {
    if (_clock_manager_state == CLOCK_MANAGER_WORKING) {
      if (_rtc_offboard.setEpoch(epoch, false, 0) == false) {
        // Failed to set time
        FAULT_SET(FAULT_RV8803_RTC_SET_TIME_FAULT);
        _clock_manager_state = CLOCK_MANAGER_TIMEOUT;
        Serial.println("ERROR: Failed to set RTC time Epoch!");
        return false;
      } else {
        // Time set successfully
        return true;
      }
    } else {
      // Clock manager is offline
      return false;
    }
  }


  uint8_t Get_Wake_Hour() {
    return _wake_hour;
  }


  void Set_Wake_Hour(uint8_t hour) {
    _wake_hour = hour;
  }


  uint8_t Get_Wake_Min() {
    return _wake_min;
  }


  void Set_Wake_Min(uint8_t min) {
    _wake_min = min;
  }


  uint8_t Get_Sleep_Hour() {
    return _sleep_hour;
  }


  void Set_Sleep_Hour(uint8_t hour) {
    _sleep_hour = hour;
  }


  uint8_t Get_Sleep_Min() {
    return _sleep_min;
  }


  void Set_Sleep_Min(uint8_t min) {
    _sleep_min = min;
  }

protected:

  //
  // Detect if the current time is between the sleep and wake window.
  // We must cover the wrap around condition.  All times are expressed in 24hr.
  // Example: wake at 07:00, sleep at 17:00
  //          At 06:59 sleep = true
  //          At 07:01 sleep = false
  //          At 16:59 sleep = false
  //          At 17:01 sleep = true
  //          At 23:59 sleep = true
  //          At 00:00 sleep = true
  //
  bool detect_sleep_window() {
    bool in_wake_window = false;

    // Create a unified representation for current time and the sleep/wake time in seconds
    // so we can compare them quickly.
    uint32_t time_seconds = _rtc_hours * 3600 + _rtc_minutes * 60 + _rtc_seconds;
    uint32_t wake_seconds = _wake_hour * 3600 + _wake_min * 60;
    uint32_t sleep_seconds = _sleep_hour * 3600 + _sleep_min * 60;

    if (wake_seconds <= sleep_seconds) {
      if ((time_seconds >= wake_seconds) && (time_seconds <= sleep_seconds)) {
        // We are in between the wake and sleep window
        in_wake_window = true;
      }
    } else {
      if ((time_seconds >= wake_seconds) || (time_seconds <= sleep_seconds)) {
        // We are in between the wake and sleep window
        in_wake_window = true;
      }
    }

    // Return if we should be sleeping
    return (!in_wake_window);
  }
};

#endif
