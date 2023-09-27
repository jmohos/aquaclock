/*
 * Aqua Clock fault definitions
 *
 * Captures the master system fault definitions.  A 32-bit system_fault parameter holds the
 * bit map status of each fault.  If a corresponding bit is set then the fault is active.
 * Any active fault will result in the Aqua Clock stopping operations since we cannot rely
 * on the inputs or the ability to regulate the water levels.
 * To set a fault, use the FAULT_SET(x) macro.
 * To print the faults there are routines in both the UIManager.h (on UI) and Aqua_Clock.ini (on console).
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef FAULTS_H
#define FAULTS_H

typedef enum {
  FAULT_I2C_MUX_OFFLINE = 0,               /* See main .ini */
  FAULT_I2C_MUX_PORT_0_FAIL = 1,           /* See main .ini */
  FAULT_I2C_MUX_PORT_1_FAIL = 2,           /* See main .ini */
  FAULT_I2C_MUX_PORT_2_FAIL = 3,           /* See main .ini */
  FAULT_VL53L1X_SENSOR_1_INIT_FAIL = 4,    /* See RangeUtil.h */
  FAULT_VL53L1X_SENSOR_2_INIT_FAIL = 5,    /* See RangeUtil.h */
  FAULT_VL53L1X_SENSOR_3_INIT_FAIL = 6,    /* See RangeUtil.h */
  FAULT_VL53L1X_UNKNOWN_INIT_FAIL = 7,     /* See RangeUtil.h */
  FAULT_VL53L1X_SENSOR_1_TIMEOUT = 8,      /* See RangeUtil.h */
  FAULT_VL53L1X_SENSOR_2_TIMEOUT = 9,      /* See RangeUtil.h */
  FAULT_VL53L1X_SENSOR_3_TIMEOUT = 10,     /* See RangeUtil.h */
  FAULT_VL53L1X_UNKNOWN_TIMEOUT = 11,      /* See RangeUtil.h */
  FAULT_RV8803_RTC_INIT_FAIL = 12,         /* See ClockManager.h */
  FAULT_RV8803_RTC_READ_FAULT = 13,        /* See ClockManager.h */
  FAULT_RV8803_RTC_SET_TIME_FAULT = 14,    /* See ClockManager.h */
  FAULT_SX1509_IO_EXPANDER_INIT_FAIL = 15, /* See main .ini */
  FAULT_TANK_FILL_TIMEOUT = 16,            /* See TankManager.h */
  FAULT_TANK_LEVEL_SENSE_FAIL = 17,        /* See TankManager.h */
  FAULT_NVM_FAIL = 18,                     /* See UIManager.h */
  FAULT_MAX_INDEX = 19
} SYSTEM_FAULT_T;

// Forward reference to master system fault bits in master .ini file.
extern uint32_t system_faults;
extern const char *FAULT_STRING[];

// Fault macros.  Replace x with the fault enumeration name.
#define FAULT_SET(x) system_faults = (system_faults | ((uint32_t)1 << x))
#define FAULT_CLEAR(x) system_faults = (system_faults &= ~((uint32_t)1 << x))
#define FAULT_ACTIVE(x) ((system_faults & (uint32_t)1 << x) != 0)

#endif