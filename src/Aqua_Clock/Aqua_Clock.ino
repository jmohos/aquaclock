/*
 * Joe Mohos' Aqua Clock Water Clock Project
 *
 * Date: June 2023
 * Board: Sparkfun ESP32 Thing Plus C
 * IDE: Arduino 2.1.0+
 *
 * Description:
 * This project is a kinetic clock that tells time using three columns of water with floating markers.
 * Left column is the Hours (1-12)
 * Middle column is the Minutes 10s digit (0-5)
 * Right column is the Minutes 1s digit (0-9)
 * 
 * Design Details:
 * A set of reservoir tanks hold water.
 * The top tank (feeder) is elevated above all the columns.
 * The bottom tank (holding) is elevated below all of the columns.
 * Gravity is used to flow the water from the feeder tank to the individual columns.
 * Gravity is used to drain the water from individual columns to the holding tank.
 * A set of 12V water solenoid valves control the added and subtracted water.
 * Each column has a single Time of Flight distance sensor to measure the column water height.
 * RangeUtil objects manage each column distance sensor.
 * Since each distance sensor uses the same I2C address, there is an IO Mux betwee the ESP32
 * and the sensors to select one device at a time.
 * ColumnManager objects manage each of the columns by controlling the valves based on the measured column elevations.
 * Each distance sensor can be calibrated for a more consistent reading of its column.  See calibration.h for static tables.
 * A 12V water pump is used to pipe from the holding tank to the feeder tank.
 * The upper tank has two water level sensors located at the 25 and 75 percent points.
 * A TankManager object manages the pump control based on the two water level sensors.
 * A battery backed I2C real-time clock (RV8803) is used as the official timekeeper.
 * A ClockManager object manages the I2C real-time clock to read and set the time.
 * The system monitors for a variety of potential failures which will stop any water flows.
 * All faults are collected into a single bit-encoded object with fault enums in faults.h.
 * All I/O is managed using an I2C IO expander (SX1509).
 * All IO expander pin definitions are captured in io_expander_config.h.
 * There are 6 12V water solenoid valves wired to a pair of 4 channel mosfet boards that switch 12V to
 * each solenoid.  The IO expander is used to output the 6 drive lines to the mosfet board.
 * For each column there is one fill and one drain valve.
 * All ESP32 microcontroller pins are mapped in pins.h.
 * For diagnostics a serial port console is created to allow for getting status and issuing commands.
 * The console runs at 115200 baud on the primary USB serial port.  Commands need to be sent with a carriage return.
 * Use the HELP command to get a list of comamnds supported.
 * A UI handler is created to manage the user interface.  The UI is a combination of a 128x128 RGB OLED display
 * and a 5 button keypad with keys for UP, DOWN, LEFT, RIGHT and SELECT.
 * The display, SSD1351, 128,128 pixel OLED display wired to the primary SPI interface of the ESP32.
 * The keypad is wired to the inputs of the IO expander.
 *
 * Arduino Project Prerequisits:
 *   ESP32 for Arduino (For Sparkfun ESP32 Thing Plus C)
 *   Preferences library for ESP32, for EEPROM save/restore.
 *   VL53L1X library for Arduino by Pololu
 *   Sparkfun Qwiic I2C Mux Arduino Library
 *   Sparkfun Qwiic RTC RV8803 Arduino Library
 *   Sparkfun SX1509 IO Expander
 *   LED Display Support
 *     Adafruit GFX library (V1.11.7+)
 *     Adafruit SSD1351
 *   elapsedMillis
 *   InterpolationLib by Luis Llamas.  (Must use V1.0.1 or greater for ESP32! See libs folder)
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
*/
#include <Arduino.h>
#include <InterpolationLib.h>
#include <Preferences.h>
#include <Wire.h>

#include <SparkFun_I2C_Mux_Arduino_Library.h>  //Click here to get the library: http://librarymanager/All#SparkFun_I2C_Mux
#include <VL53L1X.h>                           /* Pololu VL32L1X Library */
#include "calibration.h"
#include "faults.h"
#include "io_expander_config.h"
#include "ClockManager.h"
#include "ColumnManager.h"
#include "Console.h"
#include "RangeUtil.h"
#include "TankManager.h"
#include "UIManager.h"



//
// I2C Devices
//
QWIICMUX i2c_mux;   /* Used to access multiple ToF sensors with same I2C address */
SX1509 io_expander; /* Discrete I/O interface */
VL53L1X range_sensor_hour_column;
VL53L1X range_sensor_min_10s_column;
VL53L1X range_sensor_min_1s_column;
RangeUtil range_util_hour_column(&range_sensor_hour_column, 1);        // Sensor #1
RangeUtil range_util_min_10s_column(&range_sensor_min_10s_column, 2);  // Sensor #2
RangeUtil range_util_min_1s_column(&range_sensor_min_1s_column, 3);    // Sensor #3


//
// Console handler
//
Console console;

//
// User interface handler
//
UIManager *ui_manager;

//
// Tank and water column handlers
//
TankManager *tank_manager;
ColumnManager *column_manager_hour;
ColumnManager *column_manager_min_10s;
ColumnManager *column_manager_min_1s;

const uint16_t MIN_WATER_COLUMN_ELEVATION = 50;
const uint16_t MAX_WATER_COLUMN_ELEVATION = 305;

//
// Define time management elements.
//
ClockManager clock_manager;

// Status streamer
bool stream_status = false;
elapsedMillis stream_status_elapsed;
static constexpr int STREAM_STATUS_PERIOD_MS = 250;

// Heartbeat LED
elapsedMillis heartbeat_elapsed;
static constexpr int HEARTBEAT_LED_PERIOD_MS = 500;

// Bit encoded system fault status and string representations.  See faults.h for bit defintions.
uint32_t system_faults = 0;
const char *FAULT_STRING[] = {
  "I2C_MUX_OFFLINE",
  "I2C_MUX_PORT_0_FAIL",
  "I2C_MUX_PORT_1_FAIL",
  "I2C_MUX_PORT_2_FAIL",
  "VL53L1X_SENSOR_1_INIT_FAIL",
  "VL53L1X_SENSOR_2_INIT_FAIL",
  "VL53L1X_SENSOR_3_INIT_FAIL",
  "VL53L1X_UNKNOWN_INIT_FAIL",
  "L53L1X_SENSOR_1_TIMEOUT",
  "VL53L1X_SENSOR_2_TIMEOUT",
  "VL53L1X_SENSOR_3_TIMEOUT",
  "VL53L1X_UNKNOWN_TIMEOUT",
  "RV8803_RTC_INIT_FAIL",
  "RV8803_RTC_READ_FAULT",
  "RV8803_RTC_SET_TIME_FAULT",
  "SX1509_IO_EXPANDER_INIT_FAIL",
  "TANK_FILL_TIMEOUT",
  "TANK_LEVEL_SENSE_FAIL",
  "NVM_FAIL"
};



//
// Master project setup entry.  Called once right after powerup.
//
void setup() {
  //
  // CPU diagnostic light setup
  //
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //
  // Serial Port Config
  //
  Serial.begin(115200);
  Serial.println("Bootup of the AquaClock...");

  //
  // I2C bus initialization.  Used for RTC, IO Expander, Mux, Range Sensors, etc.
  //
  Wire.begin();
  Wire.setClock(400000);


  //
  // Real-time clock initialization.  Used to maintain time in battery backed clock outside of CPU.
  //
  clock_manager.Startup();

  //
  // Initialize the command console via the main serial port for diagnostics, calibration, status checks, etc.
  //
  console.Setup(&Serial);
  console.SetConsoleRxCallback(&console_rx_event);

  //
  // I2C IO Expander initialization.  SX1509 chip used for 8 inputs and 8 outputs.
  //
  if (io_expander.begin(SX1509_ADDRESS) == false) {
    // We faild to find the IO expander
    FAULT_SET(FAULT_SX1509_IO_EXPANDER_INIT_FAIL);
    Serial.println("ERROR: Failed to start SX1509 IO Expander!");
  } else {
    // Configure the IO expander input bank (pins 0-7)
    for (int i = 0; i < 8; i++) {
      io_expander.pinMode(i, INPUT_PULLUP);
    }

    // Configure the IO expander output bank (pins 8-15)
    for (int i = 8; i < 16; i++) {
      io_expander.pinMode(i, OUTPUT);
    }
  }

  //
  // I2C Mux initialization.  Used to access a multitude of I2C sensors with the same address.
  //
  if (i2c_mux.begin() == false) {
    FAULT_SET(FAULT_I2C_MUX_OFFLINE);
    Serial.println("ERROR: I2C Mux not detected!");
  }

  //
  // I2C ranging device initialization. Time-of-flight sensors to measure distance in each column.
  // Each range sensor uses the same I2C address so a mux is needed to limit coms with only one
  // sensor at a time.  Hour sensor is on mux port 0, etc.
  //
  if (!FAULT_ACTIVE(FAULT_I2C_MUX_OFFLINE)) {
    if (i2c_mux.setPort(0) == false) {
      // Hour mux port failed
      FAULT_SET(FAULT_I2C_MUX_PORT_0_FAIL);
    } else {
      // Initialize the Hour column range sensor on i2c mux port 0
      range_util_hour_column.Startup();
    }
    if (i2c_mux.setPort(1) == false) {
      // Minutes 10s mux port failed
      FAULT_SET(FAULT_I2C_MUX_PORT_1_FAIL);
    } else {
      // Initialize the Minute 10s column range sensor on i2c mux port 1
      range_util_min_10s_column.Startup();
    }
    if (i2c_mux.setPort(2) == false) {
      // Minutes 1s mux port failed
      FAULT_SET(FAULT_I2C_MUX_PORT_2_FAIL);
    } else {
      // Initialize the Minute 1s column range sensor on i2c mux port 2
      range_util_min_1s_column.Startup();
    }
  } else {
    Serial.print("Skipping range sensor inits due to missing I2C Mux...");
  }

  //
  // Create and configure the tank manager and column handlers
  // These objects regulate the functions of the tank refill pump and the column fill and drain valves.
  //
  tank_manager = new TankManager(&io_expander,
                                 SC1509_PIN_FEED_PUMP,
                                 SX1509_PIN_WATER_LOW,
                                 SC1509_PIN_WATER_HIGH);
  column_manager_hour = new ColumnManager(1,
                                          &io_expander,
                                          SC1509_PIN_HOUR_FEED,
                                          SC1509_PIN_HOUR_DRAIN,
                                          MIN_WATER_COLUMN_ELEVATION,
                                          MAX_WATER_COLUMN_ELEVATION);
  column_manager_min_10s = new ColumnManager(2,
                                             &io_expander,
                                             SC1509_PIN_MIN_10s_FEED,
                                             SC1509_PIN_MIN_10s_DRAIN,
                                             MIN_WATER_COLUMN_ELEVATION,
                                             MAX_WATER_COLUMN_ELEVATION);
  column_manager_min_1s = new ColumnManager(3,
                                            &io_expander,
                                            SC1509_PIN_MIN_1s_FEED,
                                            SC1509_PIN_MIN_1s_DRAIN,
                                            MIN_WATER_COLUMN_ELEVATION,
                                            MAX_WATER_COLUMN_ELEVATION);


  //
  // Create and start the UI handler
  // It needs access to the sensors, regulators and clock.
  //
  ui_manager = new UIManager(&io_expander,
                             &range_util_hour_column,
                             &range_util_min_10s_column,
                             &range_util_min_1s_column,
                             column_manager_hour,
                             column_manager_min_10s,
                             column_manager_min_1s,
                             tank_manager,
                             &clock_manager);
  ui_manager->Startup();

  //
  // Activate clock mode as the default
  //
  ui_manager->Set_Operating_Mode(UIManager::OPERATING_MODE_CLOCK);
}


//
// After setup this function is repeatedly called at the fastest rate possible by the processor.
// Rate control must be done in each component being updated.
//
void loop() {

  //
  // Run the console interface and process commands
  //
  console.Loop();

  //
  // Real-Time Clock read
  //
  clock_manager.Update();

  // Process the user interface menu
  ui_manager->Update();


  //
  // Compute the desired float marker elevations for the Hour, Min_10s and Min_1s columns.
  // This is done by taking the time and splitting it into the relevant digits and scaling
  // to a range sensor target.
  // There are provisions to override the target setpoint in the console for tuning purposes.
  //
  uint16_t hour_setpoint;
  if (ui_manager->Get_Hour_Column_Override_Setpoint_Enable()) {
    // User the diagnostic override value for the hour column setpoint
    hour_setpoint = ui_manager->Get_Hour_Column_Override_Setpoint();
  } else if (clock_manager.Is_Sleep_Time()) {
    // Use the sleep mode setpoint for the hour column setpoint
    hour_setpoint = 300;
  } else {
    // Use the scaled clock time to drive the hour column setpoint
    hour_setpoint = scale_hours_to_elevation(clock_manager.Get_Hour());
  }

  uint16_t min_10s_setpoint;
  if (ui_manager->Get_Min_10s_Column_Override_Setpoint_Enable()) {
    // User the diagnostic override value for the min 10s column setpoint
    min_10s_setpoint = ui_manager->Get_Min_10s_Column_Override_Setpoint();
  } else if (clock_manager.Is_Sleep_Time()) {
    // Use the sleep mode setpoint for the min 10s column setpoint
    min_10s_setpoint = 300;
  } else {
    // Use the scaled clock time to drive the min 10s column setpoint
    min_10s_setpoint = scale_minutes_10s_to_elevation(clock_manager.Get_Minute());
  }

  uint16_t min_1s_setpoint;
  if (ui_manager->Get_Min_1s_Column_Override_Setpoint_Enable()) {
    // User the diagnostic override value for the min 1s column setpoint
    min_1s_setpoint = ui_manager->Get_Min_1s_Column_Override_Setpoint();
  } else if (clock_manager.Is_Sleep_Time()) {
    // Use the sleep mode setpoint for the min 1s column setpoint
    min_1s_setpoint = 300;
  } else {
    // Use the scaled clock time to drive the min 1s column setpoint
    min_1s_setpoint = scale_minutes_1s_to_elevation(clock_manager.Get_Minute());
  }

  //
  // Update sensor readings and linearize with calibration table
  //
  i2c_mux.setPort(0);
  range_util_hour_column.Update();
  // Linearize this column raw reading
  double raw_median_range = (double)range_util_hour_column.Get_Median_Reading();
  double linearized_range = Interpolation::Linear(hour_range_cal_x_values,
                                                  hour_range_cal_y_values,
                                                  NUM_RANGE_CAL_BREAKS,
                                                  raw_median_range,
                                                  true);
  range_util_hour_column.Set_Linearized_Median_Reading((uint16_t)linearized_range);
  //Serial.print("  LINEARIZE ");
  //Serial.print(raw_median_range);
  //Serial.print(" to ");
  //Serial.println(linearized_range);

  i2c_mux.setPort(1);
  range_util_min_10s_column.Update();
  // Linearize this column raw reading
  raw_median_range = (double)range_util_min_10s_column.Get_Median_Reading();
  linearized_range = Interpolation::Linear(min_10s_range_cal_x_values,
                                           min_10s_range_cal_y_values,
                                           NUM_RANGE_CAL_BREAKS,
                                           raw_median_range,
                                           true);
  range_util_min_10s_column.Set_Linearized_Median_Reading((uint16_t)linearized_range);
  //Serial.print("  LINEARIZE ");
  //Serial.print(raw_median_range);
  //Serial.print(" to ");
  //Serial.println(linearized_range);

  i2c_mux.setPort(2);
  range_util_min_1s_column.Update();
  // Linearize this column raw reading
  raw_median_range = (double)range_util_min_1s_column.Get_Median_Reading();
  linearized_range = Interpolation::Linear(min_1s_range_cal_x_values,
                                           min_1s_range_cal_y_values,
                                           NUM_RANGE_CAL_BREAKS,
                                           raw_median_range,
                                           true);
  range_util_min_1s_column.Set_Linearized_Median_Reading((uint16_t)linearized_range);
  //Serial.print("  LINEARIZE ");
  //Serial.print(raw_median_range);
  //Serial.print(" to ");
  //Serial.println(linearized_range);

  //
  // Process each water column regulator with the latest column elevation, setpoint and override requests.
  // Support overrides for turning the drain and fill valves on for maintenance.
  //
  bool busy = column_manager_hour->Update(range_util_hour_column.Get_Linearized_Median_Reading(),
                                          hour_setpoint);

  busy |= column_manager_min_10s->Update(range_util_min_10s_column.Get_Linearized_Median_Reading(),
                                         min_10s_setpoint);

  busy |= column_manager_min_1s->Update(range_util_min_1s_column.Get_Linearized_Median_Reading(),
                                        min_1s_setpoint);

  // Process the tank_manager manager
  //if (!busy)
  //{
  tank_manager->Update();
  //}


  //
  // Stream a status line to the console for tuning if enabled in the console.
  //
  if (stream_status) {
    if (stream_status_elapsed >= STREAM_STATUS_PERIOD_MS) {
      stream_status_elapsed = 0;
      report_status_stream();
    }
  }


  //
  // React to system faults
  //
  if (system_faults != 0x0000) {
    // Disable the regulators to prevent water/pump actuation with bad sensor inputs
    tank_manager->Set_Regulator_Enable(false);
    column_manager_hour->Set_Regulator_Enable(false);
    column_manager_min_10s->Set_Regulator_Enable(false);
    column_manager_min_1s->Set_Regulator_Enable(false);
  }


  //
  // Toggle hearthbeat/fault status LED
  // Blink out a status, either healthy or a fault code in case a major component failure is detected.
  //
  if (heartbeat_elapsed >= HEARTBEAT_LED_PERIOD_MS) {
    heartbeat_elapsed = 0;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  //TODO: Add fault blink pattern generator
}


//
//
//
uint16_t scale_hours_to_elevation(uint16_t hours) {
  if (hours > 12) {
    hours = hours - 12;
  }

  // Scale 1-12 hours into hour_col_digit_elevations[NUM_HOUR_STEPS]
  if ((hours >= 1) && (hours <= 12)) {
    uint16_t elevation = hour_col_digit_elevations[hours - 1];
    return elevation;
  } else {
    // Invalid time, set to safe value
    return 150;
  }
}


//
//
//
uint16_t scale_minutes_10s_to_elevation(uint16_t minutes) {
  // Scale minutes 10s 0-5 into min_10s_col_digit_elevations[NUM_MIN_10S_STEPS]
  if ((minutes >= 0) && (minutes <= 59)) {
    uint16_t min_10s = minutes / 10;
    uint16_t elevation = min_10s_col_digit_elevations[min_10s];
    return elevation;
  } else {
    // Invalid time, set to safe value
    return 150;
  }
}

//
//
//
uint16_t scale_minutes_1s_to_elevation(uint16_t minutes) {
  // Scale minutes 1s 0-9 into elevation steps in min_1s_col_digit_elevations[NUM_MIN_1S_STEPS]
  if ((minutes >= 0) && (minutes <= 59)) {
    uint16_t min_1s = minutes - ((minutes / 10) * 10);
    uint16_t elevation = min_1s_col_digit_elevations[min_1s];
    return elevation;
  } else {
    // Invalid time, set to safe value
    return 150;
  }
}


//
//
//
void print_tank_state(TankManager *tank_manager) {
  TankManager::TANK_STATE_TYPE_T state = tank_manager->Get_State();
  switch (state) {
    case TankManager::TANK_IDLE:
      Serial.print("IDLE");
      break;

    case TankManager::TANK_FILL_ACTIVE:
      Serial.print("FILL_ACTIVE");
      break;

    case TankManager::TANK_FILL_SETTLE:
      Serial.print("FILL_SETTLE");
      break;

    case TankManager::TANK_MANUAL_FILL:
      Serial.print("MANUAL_FILL");
      break;

    case TankManager::TANK_FILL_TIMEOUT_FAULT:
      Serial.print("TANK_FILL_TIMEOUT");
      break;

    default:
      Serial.print("UNKNOWN");
      break;
  }
}




//
//
//
void print_column_state(ColumnManager *column) {
  ColumnManager::COLUMN_STATE_TYPE_T state = column->Get_State();
  switch (state) {
    case ColumnManager::COLUMN_IDLE:
      Serial.print("IDLE");
      break;

    case ColumnManager::COLUMN_DRAIN_ACTIVE:
      Serial.print("DRAIN_ACTIVE");
      break;

    case ColumnManager::COLUMN_DRAIN_SETTLE:
      Serial.print("DRAIN_SETTLE");
      break;

    case ColumnManager::COLUMN_FILL_ACTIVE:
      Serial.print("FILL_ACTIVE");
      break;

    case ColumnManager::COLUMN_FILL_SETTLE:
      Serial.print("FILL_SETTLE");
      break;

    case ColumnManager::COLUMN_MANUAL_DRAIN:
      Serial.print("MANUAL_DRAIN");
      break;

    case ColumnManager::COLUMN_MANUAL_FILL:
      Serial.print("MANUAL_FILL");
      break;

    case ColumnManager::COLUMN_ERROR_STATE:
      Serial.print("COLUMN_ERROR");
      break;

    default:
      Serial.print("UNKNOWN");
      break;
  }
}


//
//
//
void print_column_control_error_state(ColumnManager *column) {
  ColumnManager::CONTROL_ERROR_STATE_TYPE_T control_error_state = column->Get_Control_Error_State();
  switch (control_error_state) {
    case ColumnManager::CONTROL_ERROR_DEADBAND:
      Serial.print("IN_DEADBAND");
      break;
    case ColumnManager::CONTROL_ERROR_POSITIVE:
      Serial.print("POSITIVE");
      break;
    case ColumnManager::CONTROL_ERROR_NEGATIVE:
      Serial.print("NEGATIVE");
      break;
    default:
      Serial.print("UNKNOWN");
      break;
  }
}


//
//
//
void print_active_faults() {
  for (int i = 0; i < FAULT_MAX_INDEX; i++) {
    if (FAULT_ACTIVE(i)) {
      Serial.println(FAULT_STRING[i]);
    }
  }
}


//
// Report a stream status update on the serial console.
// Print a header and then a line of the values.
//
void report_status_stream() {
  // Header
  //Serial.println("TIME, RTC_Online, H_ELEV, M10s_ELEV, M1s_ELEV, H_Set, M_10s_Set, M_1s_Set, TankState, HrState, Min10State, Min1State");

  // Time
  Serial.print(clock_manager.Get_Hour());
  Serial.print(":");
  Serial.print(clock_manager.Get_Minute());
  Serial.print(":");
  Serial.print(clock_manager.Get_Second());
  Serial.print(" ");
  Serial.print(clock_manager.Get_Year());
  Serial.print("-");
  Serial.print(clock_manager.Get_Month());
  Serial.print("-");
  Serial.print(clock_manager.Get_Day());


  // RTC Online
  Serial.print(", ");
  if (clock_manager.Is_Working()) {
    Serial.print("CLK_OK, ");
  } else {
    Serial.print("BAD_CLK, ");
  }

  // Print tank_manager elevation detections
  if (tank_manager->Is_Feed_Tank_Above_Low_Mark()) {
    Serial.print("TANK_LOW=WET, ");
  } else {
    Serial.print("TANK_LOW=DRY, ");
  }
  if (tank_manager->Is_Feed_Tank_Above_High_Mark()) {
    Serial.print("TANK_HIGH=WET, ");
  } else {
    Serial.print("TANK_HIGH=DRY, ");
  }

  // Column Elevations and setpoints and errros
  uint16_t elev_raw = range_util_hour_column.Get_Median_Reading();
  uint16_t elev_lin = range_util_hour_column.Get_Linearized_Median_Reading();
  uint16_t setpoint = column_manager_hour->Get_Target_Setpoint_MM();
  int16_t error = setpoint - elev_lin;
  Serial.print("(");
  Serial.print(elev_raw);
  Serial.print(", ");
  Serial.print(elev_lin);
  Serial.print(", ");
  Serial.print(setpoint);
  Serial.print(", ");
  Serial.print(error);
  Serial.print("), ");

  elev_raw = range_util_min_10s_column.Get_Median_Reading();
  elev_lin = range_util_min_10s_column.Get_Linearized_Median_Reading();
  setpoint = column_manager_min_10s->Get_Target_Setpoint_MM();
  error = setpoint - elev_lin;
  Serial.print("(");
  Serial.print(elev_raw);
  Serial.print(", ");
  Serial.print(elev_lin);
  Serial.print(", ");
  Serial.print(setpoint);
  Serial.print(", ");
  Serial.print(error);
  Serial.print("), ");

  elev_raw = range_util_min_1s_column.Get_Median_Reading();
  elev_lin = range_util_min_1s_column.Get_Linearized_Median_Reading();
  setpoint = column_manager_min_1s->Get_Target_Setpoint_MM();
  error = setpoint - elev_lin;
  Serial.print("(");
  Serial.print(elev_raw);
  Serial.print(", ");
  Serial.print(elev_lin);
  Serial.print(", ");
  Serial.print(setpoint);
  Serial.print(", ");
  Serial.print(error);
  Serial.print("), ");

  Serial.println();
}



//
//   Console input event handler
//
//   This callback is called when a line of console input is received
//   from the primary console UART.  This can come from the UART port
//   or the USB serial if enabled.
//
//   The expected line format is:
//      command, parameter1, parameter2
//      STEPPER, STATUS, 0
//
// Console inputs
//
void console_rx_event(String command, String param1, String param2) {
  // Uncomment to see details of input
  //Serial.println("GOT Command: " + command + ", " + param1 + ", " + param2);

  if (command.equals("HELP")) {
    Serial.println("Joe's Aqua Clock HELP options:");
    Serial.println("--------------------------------------------------------");
    Serial.println("   STATUS            - Report status block");
    Serial.println("   MODE x            - Set mode, x=CLOCK or STATIC or VALVE");
    Serial.println("   FILL  x period    - Fill device x for period msec, 0=tank_manager,1=hr,2&3=min");
    Serial.println("   DRAIN x period    - Drain column x for period msec");
    Serial.println("   LOGON x           - Enable logging, 0=tank_manager,1=hr,2&3=min");
    Serial.println("   LOGOFF x          - Disable logging, 0=tank_manager,1=hr,2&3=min");
    Serial.println("   STREAMON          - Enable periodic status streaming");
    Serial.println("   STREAMOFF         - Disable periodic status streaming");
    Serial.println("   OVERRIDE x value  - Adjust setpoint for x to value, 1=hr,2&3=min");
    Serial.println("   ENABLE x          - Enable regulator, 0=tank_manager,1=hr,2&3=min");
    Serial.println("   DISABLE x         - Disable regulator, 0=tank_manager,1=hr,2&3=min");
    Serial.println("   TIME READ         - Report time in HH:MM:SS");
    Serial.println("   TIME SET x        - Set time to epoch x");
    Serial.println("   RESTART           - Reboot the controller");
    Serial.println("--------------------------------------------------------");
  } else if (command == "STATUS") {
    /* Dump Status Block */
    Serial.printf("Faults: = %08lx\n", system_faults);
    Serial.println();

    print_active_faults();
    Serial.println();
    Serial.println("<<<<--Tank Status-->>>>");
    Serial.print("   Tank State: ");
    print_tank_state(tank_manager);
    Serial.println();
    Serial.print("   Tank Regulator Enable: ");
    Serial.print(tank_manager->Is_Tank_Regulator_Enabled());
    Serial.println();
    Serial.print("   Feed Tank High,Low Levels: ");
    Serial.print(tank_manager->Is_Feed_Tank_Above_High_Mark());
    Serial.print(", ");
    Serial.println(tank_manager->Is_Feed_Tank_Above_Low_Mark());

    Serial.println("<<<<--Hour Column Status-->>>>");
    Serial.print("   Column State: ");
    print_column_state(column_manager_hour);
    Serial.println();
    Serial.print("   Column Regulator Enable: ");
    Serial.println(column_manager_hour->Is_Column_Regulator_Enabled());
    Serial.print("   Column Error State: ");
    print_column_control_error_state(column_manager_hour);
    Serial.println();
    Serial.print("   Setpoint: ");
    Serial.print(column_manager_hour->Get_Target_Setpoint_MM());
    Serial.print("   Elevation: ");
    Serial.print(column_manager_hour->Get_Elevation_Reading_MM());
    Serial.println();

    Serial.println("<<<<--Min 10s Column Status-->>>>");
    Serial.print("   Column State: ");
    print_column_state(column_manager_min_10s);
    Serial.println();
    Serial.print("   Column Regulator Enable: ");
    Serial.println(column_manager_min_10s->Is_Column_Regulator_Enabled());
    Serial.print("   Column Error State: ");
    print_column_control_error_state(column_manager_min_10s);
    Serial.println();
    Serial.print("   Setpoint: ");
    Serial.print(column_manager_min_10s->Get_Target_Setpoint_MM());
    Serial.print("   Elevation: ");
    Serial.print(column_manager_min_10s->Get_Elevation_Reading_MM());
    Serial.println();

    Serial.println("<<<<--Min 1s Column Status-->>>>");
    Serial.print("   Column State: ");
    print_column_state(column_manager_min_1s);
    Serial.println();
    Serial.print("   Column Regulator Enable: ");
    Serial.println(column_manager_min_1s->Is_Column_Regulator_Enabled());
    Serial.print("   Column Error State: ");
    print_column_control_error_state(column_manager_min_1s);
    Serial.println();
    Serial.print("   Setpoint: ");
    Serial.print(column_manager_min_1s->Get_Target_Setpoint_MM());
    Serial.print("   Elevation: ");
    Serial.print(column_manager_min_1s->Get_Elevation_Reading_MM());
    Serial.println();

  } else if (command == "MODE") {
    /*
     * Expecting "MODE CLOCK" or "MODE STATIC" or "MODE VALVE"
     * Results in operating mode adjustment in UI
     */
    if (param1 == "CLOCK") {
      Serial.println("   Setting mode to CLOCK.");
      ui_manager->Set_Operating_Mode(UIManager::OPERATING_MODE_CLOCK);
    } else if (param1 == "STATIC") {
      Serial.println("   Setting mode to STATIC OVERRIDE.");
      ui_manager->Set_Operating_Mode(UIManager::OPERATING_MODE_STATIC_OVERRIDE);
    } else if (param1 == "VALVE") {
      Serial.println("   Setting mode to manual VALVE OVERRIDE.");
      ui_manager->Set_Operating_Mode(UIManager::OPERATING_MODE_VALVE_OVERRIDE);
    } else {
      Serial.println("ERROR: Unsupported mode!");
    }
  } else if (command == "FILL") {
    /*
     * Expecting "FILL 1 2000"
     * Results in manual Fill of column 1 for 2 seconds.
     */
    //unsigned long period = strtol(param2.c_str(), NULL, 10);
    uint32_t period = param2.toInt();
    if (param1 == "0") {
      Serial.println("  Manually filling feed tank_manager->..");
      tank_manager->Manual_Fill(period);
    } else if (param1 == "1") {
      Serial.println("  Manually filling hour column...");
      column_manager_hour->Manual_Fill(period);
    } else if (param1 == "2") {
      Serial.println("  Manually filling min 10s column...");
      column_manager_min_10s->Manual_Fill(period);
    } else if (param1 == "3") {
      Serial.println("  Manually filling min 1s column...");
      column_manager_min_1s->Manual_Fill(period);
    } else {
      Serial.println("ERROR: Unsupported command!");
    }
  } else if (command == "DRAIN") {
    /*
     * Expecting "DRAIN 1 2000"
     * Results in manual Drain of column 1 for 2 seconds.
     */
    unsigned long period = strtol(param2.c_str(), NULL, 10);
    if (param1 == "1") {
      column_manager_hour->Manual_Drain(period);
      Serial.println("  Manually draining hour column...");
    } else if (param1 == "2") {
      column_manager_min_10s->Manual_Drain(period);
      Serial.println("  Manually draining min 10s column...");
    } else if (param1 == "3") {
      column_manager_min_1s->Manual_Drain(period);
      Serial.println("  Manually draining min 1s column...");
    } else {
      Serial.println("ERROR: Unsupported command!");
    }
  } else if (command == "LOGON") {
    /*
     * Expecting "LOGON 1"
     * Results in enable of logging for element 1.
     */
    int unit = param1.toInt();
    Serial.println("About to enable logging...");

    if ((unit >= 0) && (unit <= 3)) {
      Serial.println("Unit is valid!");
      switch (unit) {
        case 0:
          tank_manager->Enable_Logging();
          Serial.println("Enabled loggin on tank_manager->");
          break;

        case 1:
          column_manager_hour->Enable_Logging();
          Serial.println("Enabled loggin on Hour column.");
          break;

        case 2:
          column_manager_min_10s->Enable_Logging();
          Serial.println("Enabled loggin on Min 10s column.");
          break;

        case 3:
          column_manager_min_1s->Enable_Logging();
          Serial.println("Enabled loggin on Min 1s column.");
          break;
      }
    } else {
      Serial.println("Invalid unit field!");
    }
  } else if (command == "LOGOFF") {
    /*
     * Expecting "LOGOFF 1"
     * Results in disable of logging for element 1.
     */
    int unit = param1.toInt();
    if ((unit >= 0) && (unit <= 3)) {
      switch (unit) {
        case 0:
          tank_manager->Disable_Logging();
          Serial.println("Disabled loggin on tank_manager->");
          break;

        case 1:
          column_manager_hour->Disable_Logging();
          Serial.println("Disabled loggin on Hour column.");
          break;

        case 2:
          column_manager_min_10s->Disable_Logging();
          Serial.println("Disabled loggin on Min 10s column.");
          break;

        case 3:
          column_manager_min_1s->Disable_Logging();
          Serial.println("Disabled loggin on Min 1s column.");
          break;
      }
    } else {
      Serial.println("Invalid unit field!");
    }
  } else if (command == "STREAMON") {
    Serial.println("Starting status streaming...");
    stream_status = true;
  } else if (command == "STREAMOFF") {
    Serial.println("Stopping status streaming...");
    stream_status = false;
  } else if (command == "ENABLE") {
    /*
     * Expecting "ENABLE 1"
     * Results in turning on the regulator for device 1 (0=Tank, 1=Hour, 2= Min10s, 3=Min1s)
     */
    if (param1 == "0") {
      Serial.println("  Enabling tank_manager regulator...");
      tank_manager->Set_Regulator_Enable(true);
    } else if (param1 == "1") {
      Serial.println("  Enabling Hour column regulator...");
      column_manager_hour->Set_Regulator_Enable(true);
    } else if (param1 == "2") {
      Serial.println("  Enabling Minutes 10s column regulator...");
      column_manager_min_10s->Set_Regulator_Enable(true);
    } else if (param1 == "3") {
      Serial.println("  Enabling Minutes 1s column regulator...");
      column_manager_min_1s->Set_Regulator_Enable(true);
    } else {
      Serial.println("ERROR: Unsupported command!");
    }
  } else if (command == "DISABLE") {
    /*
     * Expecting "DISABLE 1"
     * Results in turning off the regulator for device 1 (0=Tank, 1=Hour, 2= Min10s, 3=Min1s)
     */
    if (param1 == "0") {
      Serial.println("  Disabling tank_manager regulator...");
      tank_manager->Set_Regulator_Enable(false);
    } else if (param1 == "1") {
      Serial.println("  Disabling Hour column regulator...");
      column_manager_hour->Set_Regulator_Enable(false);
    } else if (param1 == "2") {
      Serial.println("  Disabling Minutes 10s column regulator...");
      column_manager_min_10s->Set_Regulator_Enable(false);
    } else if (param1 == "3") {
      Serial.println("  Disabling Minutes 1s column regulator...");
      column_manager_min_1s->Set_Regulator_Enable(false);
    } else {
      Serial.println("ERROR: Unsupported command!");
    }
  } else if (command == "OVERRIDE") {
    /*
     * Expecting "OVERRIDE 1 150"
     * Results in enabling setpoint override and making hour setpoint = 150..
     */
    uint32_t value = param2.toInt();
    if (param1 == "1") {
      Serial.print("  Setting setpoint for hour column to: ");
      Serial.println(value);
      ui_manager->Set_Hour_Column_Override_Setpoint(value);
    } else if (param1 == "2") {
      Serial.print("  Setting setpoint for min 10s column to: ");
      Serial.println(value);
      ui_manager->Set_Min_10s_Column_Override_Setpoint(value);
    } else if (param1 == "3") {
      Serial.print("  Setting setpoint for min 1s column to: ");
      Serial.println(value);
      ui_manager->Set_Min_1s_Column_Override_Setpoint(value);
    } else {
      Serial.println("ERROR: Unsupported command!");
    }
  } else if (command == "TIME") {
    if (param1 == "READ") {
      Serial.print("TIME: ");
      Serial.print(clock_manager.Get_Hour());
      Serial.print(":");
      Serial.print(clock_manager.Get_Minute());
      Serial.print(":");
      Serial.println(clock_manager.Get_Second());
    } else if (param1 == "SET") {
      unsigned long epoch = strtol(param2.c_str(), NULL, 10);
      clock_manager.Set_Time_Epoch(epoch);
    } else {
      Serial.println("ERROR: Unsupported command!");
    }

  } else if (command == "RESTART") {
    ESP.restart();
  }
};
