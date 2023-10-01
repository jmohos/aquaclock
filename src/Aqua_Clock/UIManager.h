/*
 * UI Manager class for the Aqua Clock
 *
 * Manages the user interface implemented via a small OLED screen and 5 input buttons.
 * The UI is implemented using a series of states that define what should be displayed
 * and what user inputs are for that given state.  
 *
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <elapsedMillis.h>
#include <Preferences.h>
#include <SPI.h>

#include <Adafruit_GFX.h>     /* Graphics support library */
#include <Adafruit_SSD1351.h> /* OLED Display Support, 1.5", RGB, 128x128, SPI bus */

#include "pins.h"
#include "io_expander_config.h"
#include "ClockManager.h"
#include "ColumnManager.h"
#include "TankManager.h"


class UIManager {
public:

  typedef enum {
    OPERATING_MODE_CLOCK,
    OPERATING_MODE_STATIC_OVERRIDE,
    OPERATING_MODE_VALVE_OVERRIDE
  } OPERATING_MODE_T;


private:

// Display settings, 1.5" Waveshare display, SSD1351, I2C interface
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

// Color definitions in 16-bit RGB with 5 bits R, 6 bits Green, 5 bits blue.
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

// Standardized colors used throughout the UI for consistency
#define TEXT_COLOR_BASE WHITE
#define TEXT_COLOR_TITLE GREEN
#define TEXT_COLOR_HIGHLIGHT RED

  // Graphics drivers for the real display and a buffered canvas to
  // allow for drawing without flicker
  Adafruit_SSD1351 *_display;
  GFXcanvas16 *_canvas;

  // Graphic element properties
  const int16_t COLUMN_GRAPHIC_WIDTH = 18;
  const int16_t COLUMN_GRAPHIC_HEIGHT = 64;


  /* Handles to system components the UI will interact with */
  SX1509 *_io_expander;
  RangeUtil *_hour_column_range;
  RangeUtil *_min_10s_column_range;
  RangeUtil *_min_1s_column_range;
  ColumnManager *_column_manager_hour;
  ColumnManager *_column_manager_min_10s;
  ColumnManager *_column_manager_min_1s;
  TankManager *_tank;
  ClockManager *_clock_man;

  // Temporary edit parameters for UI inputs
  uint8_t _edit_field_index = 0;
  uint8_t _edit_rtc_seconds;
  uint8_t _edit_rtc_minutes;
  uint8_t _edit_rtc_hours;
  uint8_t _edit_rtc_date;
  uint8_t _edit_rtc_weekday;
  uint8_t _edit_rtc_month;
  uint16_t _edit_rtc_year;
  uint8_t _edit_wake_hour;
  uint8_t _edit_wake_min;
  uint8_t _edit_sleep_hour;
  uint8_t _edit_sleep_min;


  // Menu States
  typedef enum {
    // One time boot transition state
    MENU_STATE_0_INIT,

    // "Parking" state when not changing anything
    MENU_STATE_1_IDLE,

    // UI Menu Selection state
    MENU_STATE_2_SELECT_MENU,

    // Individual UI Menus
    MENU_STATE_3_DO_CLOCK_DIAGS,
    MENU_STATE_4_DO_SET_TIME,
    MENU_STATE_5_DO_SET_DATE,
    MENU_STATE_6_DO_SET_SLEEP,
    MENU_STATE_7_DO_MAN_VALVES,
    MENU_STATE_8_DO_MAN_PUMP,
    MENU_STATE_9_DO_MAN_SETPOINTS,
    MENU_STATE_10_DO_SET_WIFI,

  } MENU_STATE_T;
  MENU_STATE_T _menu_state = MENU_STATE_0_INIT;

  // Keyboard input debouncing support
  elapsedMillis _menu_button_debounce_period_elapsed;
  elapsedMillis _menu_state_update_period_elapsed;
  static constexpr int MENU_BUTTON_DEBOUNCE_PERIOD_MS = 50;
  static constexpr int MENU_STATE_UPDATE_PERIOD_MS = 100;

  OPERATING_MODE_T _operating_mode = OPERATING_MODE_CLOCK;

  // Water column regulator override enables and setpoints for
  // UI diagnostic actions.
  bool _hour_override_setpoint_enable = false;
  bool _min_10s_override_setpoint_enable = false;
  bool _min_1s_override_setpoint_enable = false;
  uint16_t _hour_override_setpoint = 150;
  uint16_t _min_10s_override_setpoint = 150;
  uint16_t _min_1s_override_setpoint = 150;

  // UI keyboard input parameters
  typedef struct {
    bool left_button_active = false;
    bool right_button_active = false;
    bool up_button_active = false;
    bool down_button_active = false;
    bool enter_button_active = false;
  } Buttons;
  Buttons cur_button_status;
  Buttons pre_button_status;

//Button accessor macros
// These detect level events on the button inputs.
#define LEFT_BUTTON_ACTIVE (cur_button_status.left_button_active == true)
#define RIGHT_BUTTON_ACTIVE (cur_button_status.right_button_active == true)
#define UP_BUTTON_ACTIVE (cur_button_status.up_button_active == true)
#define DOWN_BUTTON_ACTIVE (cur_button_status.down_button_active == true)
#define ENTER_BUTTON_ACTIVE (cur_button_status.enter_button_active == true)

// Button transition detection macros
// These detect edge events on the button inputs.
#define LEFT_BUTTON_PRESSED ((pre_button_status.left_button_active == false) && (cur_button_status.left_button_active == true))
#define RIGHT_BUTTON_PRESSED ((pre_button_status.right_button_active == false) && (cur_button_status.right_button_active == true))
#define UP_BUTTON_PRESSED ((pre_button_status.up_button_active == false) && (cur_button_status.up_button_active == true))
#define DOWN_BUTTON_PRESSED ((pre_button_status.down_button_active == false) && (cur_button_status.down_button_active == true))
#define ENTER_BUTTON_PRESSED ((pre_button_status.enter_button_active == false) && (cur_button_status.enter_button_active == true))

  // Non-volatile memory storage handler
  Preferences preferences;

  // Structure holding all backed up non-volatile parameters
  typedef struct {
    uint8_t wake_hour;
    uint8_t wake_min;
    uint8_t sleep_hour;
    uint8_t sleep_min;
  } NVM_PREFERENCES_T;
  NVM_PREFERENCES_T backup_settings;

  // Defaults for the non-volatile settings in case they have never
  // been programmed into the ESP32 Preferences EEPROM partitions.
  NVM_PREFERENCES_T backup_settings_defaults = {
    7,   // wake_hour
    0,   // wake_min
    19,  // sleep_hour
    0    // sleep_min
  };


public:

  /* Constructor - Capture access to all relevant system components */
  UIManager(SX1509 *io_expander, /* Button inputs */
            RangeUtil *hour_column_range,
            RangeUtil *min_10s_column_range,
            RangeUtil *min_1s_column_range,
            ColumnManager *column_manager_hour,
            ColumnManager *column_manager_min_10s,
            ColumnManager *column_manager_min_1s,
            TankManager *tank,
            ClockManager *clock_man) {
    _io_expander = io_expander;
    _hour_column_range = hour_column_range;
    _min_10s_column_range = min_10s_column_range;
    _min_1s_column_range = min_1s_column_range;
    _column_manager_hour = column_manager_hour;
    _column_manager_min_10s = column_manager_min_10s;
    _column_manager_min_1s = column_manager_min_1s;
    _tank = tank;
    _clock_man = clock_man;

    // Initiate both a real and virtual graphics interface.  They are the same size so we can
    // simply copy the canvas to the display when we are done with a UI update cycle.
    _display = new Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, SPI_OLED_CS_PIN, SPI_OLED_DC_PIN, SPI_OLED_RST_PIN);
    _canvas = new GFXcanvas16(SCREEN_WIDTH, SCREEN_HEIGHT);
  }


  //
  // Setup the UI interfaces to the display->
  //
  void Startup() {

    // Attempt to access a namespace called "aquaclock" in the ESP32 preferences EEPROM.
    // If it does not exist then create one.
    // Restore all prior saved parameters.
    // If we cannot create one then issue a system fault.
    if (!preferences.begin("aquaclock", false)) {
      FAULT_SET(FAULT_NVM_FAIL);
      Serial.println("ERROR: Failed to find Non-Volatile memory space for aquaclock!");
    } else {
      Serial.println("Opened non-volatile memory.");
      restore_preferences();
    }

    // Initialize the real display.
    _display->begin();      // One way coms so no feedback if display is not there or does not work.
    _display->cp437(true);  // Use full 256 char 'Code Page 437' font
    _display->fillScreen(BLACK);
    _display->setTextWrap(false);  // Don't allow text to wrap a line

    // Initialize the virtual display buffer canvas with the same settings.
    _canvas->cp437(true);
    _canvas->setTextWrap(false);
    _canvas->fillScreen(BLACK);
  }



  //
  // Update the UI by scanning for inputs, running the approprate state handler and determining what to show.
  //
  void Update() {
    static MENU_STATE_T prior_menu_state = MENU_STATE_0_INIT;

    debounce_buttons();

    // Rate control the UI processing
    if (_menu_state_update_period_elapsed < MENU_STATE_UPDATE_PERIOD_MS) {
      return;
    }
    _menu_state_update_period_elapsed = 0;

    detect_button_activity();


    // Before any UI state updates, clear the buffer, restore the cursor
    // set the text size to normal so we don't have to do it in each state.
    _canvas->fillScreen(BLACK);
    _canvas->setCursor(0, 0);  // Start at top-left corner
    _canvas->setTextSize(1);   // Double size

    // Keep track of state history to look for special state transitions
    prior_menu_state = _menu_state;

    // Process the state specific UI task to know what to draw, how to react
    // to keyboard inputs and what state to transition to.
    switch (_menu_state) {
      case MENU_STATE_0_INIT:
        _menu_state = do_menu_0_init_state();
        break;
      case MENU_STATE_1_IDLE:
        _menu_state = do_menu_1_idle_state();
        break;
      case MENU_STATE_2_SELECT_MENU:
        _menu_state = do_menu_2_select_menu_state();
        break;
      case MENU_STATE_3_DO_CLOCK_DIAGS:
        _menu_state = do_menu_3_clock_diags_state();
        break;
      case MENU_STATE_4_DO_SET_TIME:
        _menu_state = do_menu_4_set_time_state();
        break;
      case MENU_STATE_5_DO_SET_DATE:
        _menu_state = do_menu_5_set_date_state();
        break;
      case MENU_STATE_6_DO_SET_SLEEP:
        _menu_state = do_menu_6_set_sleep_state();
        break;
      case MENU_STATE_7_DO_MAN_VALVES:
        _menu_state = do_menu_7_man_valves_state();
        break;
      case MENU_STATE_8_DO_MAN_PUMP:
        _menu_state = do_menu_8_man_pump_state();
        break;
      case MENU_STATE_9_DO_MAN_SETPOINTS:
        _menu_state = do_menu_9_man_setpoints_state();
        break;
      case MENU_STATE_10_DO_SET_WIFI:
        _menu_state = do_menu_10_set_wifi_state();
        break;
      default:
        // Should not get here!
        _menu_state = MENU_STATE_0_INIT;
        break;
    } /* switch state */

    // Record history for button transition detection
    pre_button_status = cur_button_status;

    // After updating all display elements, transfer the contents of the virtual
    // display into the real one.  This method prevents flicker that would happen if we
    // directly modified the display element by element.
    _display->drawRGBBitmap(0, 0, _canvas->getBuffer(), _canvas->width(), _canvas->height());
  }


  OPERATING_MODE_T Get_Operating_Mode() {
    return _operating_mode;
  }


  void Set_Operating_Mode(OPERATING_MODE_T operating_mode) {
    switch (operating_mode) {
      case OPERATING_MODE_CLOCK:
        // Enable tank and column regulators
        set_tank_regulator_enable(true);
        set_hour_column_regulator_enable(true);
        set_min_10s_column_regulator_enable(true);
        set_min_1s_column_regulator_enable(true);

        // Disable column overrides
        set_hour_column_override_setpoint_enable(false);
        set_min_10s_column_override_setpoint_enable(false);
        set_min_1s_column_override_setpoint_enable(false);
        _operating_mode = operating_mode;
        break;

      case OPERATING_MODE_STATIC_OVERRIDE:
        // Enable tank and column regulators
        set_tank_regulator_enable(true);
        set_hour_column_regulator_enable(true);
        set_min_10s_column_regulator_enable(true);
        set_min_1s_column_regulator_enable(true);

        // Enable column overrides
        set_hour_column_override_setpoint_enable(true);
        set_min_10s_column_override_setpoint_enable(true);
        set_min_1s_column_override_setpoint_enable(true);
        _operating_mode = operating_mode;
        break;

      case OPERATING_MODE_VALVE_OVERRIDE:
        // Disable tank and column regulators
        set_tank_regulator_enable(false);
        set_hour_column_regulator_enable(false);
        set_min_10s_column_regulator_enable(false);
        set_min_1s_column_regulator_enable(false);

        // Disable column overrides
        set_hour_column_override_setpoint_enable(false);
        set_min_10s_column_override_setpoint_enable(false);
        set_min_1s_column_override_setpoint_enable(false);
        _operating_mode = operating_mode;
        break;

      default:
        // Disable overrides
        // Invalid request, fall back to clock mode
        _operating_mode = OPERATING_MODE_CLOCK;
        break;
    }
  }


  bool Get_Tank_Regulator_Enable() {
    return _tank->Is_Tank_Regulator_Enabled();
  }


  bool Get_Hour_Column_Regulator_Enable() {
    return _column_manager_hour->Is_Column_Regulator_Enabled();
  }


  bool Get_Hour_Column_Override_Setpoint_Enable() {
    return _hour_override_setpoint_enable;
  }


  uint16_t Get_Hour_Column_Override_Setpoint() {
    return _hour_override_setpoint;
  }


  void Set_Hour_Column_Override_Setpoint(uint16_t setpoint) {
    _hour_override_setpoint = setpoint;
  }


  bool Get_Min_10s_Column_Regulator_Enable() {
    return _column_manager_min_10s->Is_Column_Regulator_Enabled();
  }


  bool Get_Min_10s_Column_Override_Setpoint_Enable() {
    return _min_10s_override_setpoint_enable;
  }


  uint16_t Get_Min_10s_Column_Override_Setpoint() {
    return _min_10s_override_setpoint;
  }


  void Set_Min_10s_Column_Override_Setpoint(uint16_t setpoint) {
    _min_10s_override_setpoint = setpoint;
  }


  bool Get_Min_1s_Column_Regulator_Enable() {
    return _column_manager_min_1s->Is_Column_Regulator_Enabled();
  }


  bool Get_Min_1s_Column_Override_Setpoint_Enable() {
    return _min_1s_override_setpoint_enable;
  }


  uint16_t Get_Min_1s_Column_Override_Setpoint() {
    return _min_1s_override_setpoint;
  }


  void Set_Min_1s_Column_Override_Setpoint(uint16_t setpoint) {
    _min_1s_override_setpoint = setpoint;
  }

protected:

  //
  // Restore all backed up ESP32 non-volatile parameters
  //
  void restore_preferences() {

    // Attempt to read in all backup settings, apply defaults if they don't exist
    backup_settings.wake_hour = preferences.getUChar("wake_hour", backup_settings_defaults.wake_hour);
    backup_settings.wake_min = preferences.getUChar("wake_min", backup_settings_defaults.wake_min);
    backup_settings.sleep_hour = preferences.getUChar("sleep_hour", backup_settings_defaults.sleep_hour);
    backup_settings.sleep_min = preferences.getUChar("sleep_min", backup_settings_defaults.sleep_min);

    // Transfer the newly restored clock setting to the clock manager
    apply_clock_sleep_settings();
  }


  //
  // Save all backed up ESP32 non-volatile parameters
  //
  void save_preferences() {
    preferences.putUChar("wake_hour", backup_settings.wake_hour);
    preferences.putUChar("wake_min", backup_settings.wake_min);
    preferences.putUChar("sleep_hour", backup_settings.sleep_hour);
    preferences.putUChar("sleep_min", backup_settings.sleep_min);
  }


  MENU_STATE_T do_menu_0_init_state() {
    _canvas->fillScreen(BLACK);

    return MENU_STATE_1_IDLE;
  }


  MENU_STATE_T do_menu_1_idle_state() {

    _canvas->setTextSize(2);
    print_menu_header("AQUA CLOCK");
    _canvas->setTextSize(1);

    // Print date and time
    print_zero_padded_four_digit_uint(_clock_man->Get_Year(), false);
    _canvas->print(F("-"));
    print_zero_padded_two_digit_uint(_clock_man->Get_Month(), false);
    _canvas->print(F("-"));
    print_zero_padded_two_digit_uint(_clock_man->Get_Day(), false);
    _canvas->print("  ");
    print_zero_padded_two_digit_uint(_clock_man->Get_Hour(), false);
    _canvas->print(F(":"));
    print_zero_padded_two_digit_uint(_clock_man->Get_Minute(), false);
    _canvas->print(F(":"));
    print_zero_padded_two_digit_uint(_clock_man->Get_Second(), false);
    _canvas->println();
    _canvas->print(F("Mode: "));
    switch (_operating_mode) {
      case OPERATING_MODE_CLOCK:
        _canvas->println("CLOCK");
        break;

      case OPERATING_MODE_STATIC_OVERRIDE:
        _canvas->println("STATIC OVERRIDE");
        break;

      case OPERATING_MODE_VALVE_OVERRIDE:
        _canvas->println("VALVE OVERRIDE");
        break;

      default:
        _canvas->println("???");
        break;
    }

    // Report sleep status
    if (_clock_man->Is_Sleep_Time()) {
      _canvas->printf("Sleeping till ");
      print_zero_padded_two_digit_uint(backup_settings.wake_hour, false);
      _canvas->printf(":");
      print_zero_padded_two_digit_uint(backup_settings.wake_min, false);
    } else {
      _canvas->printf("Awake till ");
      print_zero_padded_two_digit_uint(backup_settings.sleep_hour, false);
      _canvas->printf(":");
      print_zero_padded_two_digit_uint(backup_settings.sleep_min, false);
    }
    _canvas->println();

    // Display any system faults
    if (system_faults != 0x0000) {
      _canvas->println();
      _canvas->setTextColor(RED, BLACK);  // Default is white text over black background
      _canvas->printf("Faults: = %08lx\r\n", system_faults);
      display_faults();
      _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Default is white text over black background
    } else {
      _canvas->printf("Faults: None\r\n");
    }

    // Draw the seconds on a progress bar to countdown changes in the clock
    float percentage = (((float)_clock_man->Get_Second() * 1.0) / 60.0f); /* % = [0.0 - 1.0] */
    draw_progress_bar(32, 90, 62, 20, percentage, WHITE, GREEN);

    // Draw instructions at the bottom
    _canvas->setCursor(0, 120);
    _canvas->print(F("Hit "));
    _canvas->write(0x1F);  // Down arrow
    _canvas->println(F(" for menu."));

    // Draw a line under menu heading
    //_canvas->drawLine(0, 10, _canvas->width() - 1, 10, WHITE);

    // Up or Down = Enter the selection menu state
    if ((DOWN_BUTTON_PRESSED) || (UP_BUTTON_PRESSED)) {
      _edit_field_index = 0; /* Begin with selection on the first choice */
      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_2_select_menu_state() {
    // Present the menu selection list.
    print_menu_header("---SELECTION MENU---");

    print_parameter("CLOCK DIAGS", true, (_edit_field_index == 0));
    print_parameter("SET TIME", true, (_edit_field_index == 1));
    print_parameter("SET DATE", true, (_edit_field_index == 2));
    print_parameter("SET SLEEP", true, (_edit_field_index == 3));
    print_parameter("MAN VALVES ", true, (_edit_field_index == 4));
    print_parameter("MAN PUMP ", true, (_edit_field_index == 5));
    print_parameter("MAN SETPOINTS", true, (_edit_field_index == 6));
    print_parameter("SET WIFI", true, (_edit_field_index == 7));

    // Down = Decrement the menu selection
    if (DOWN_BUTTON_PRESSED) {
      if (_edit_field_index < 7) {
        _edit_field_index++;
      }
    }

    // Up = Increment the menu selection
    if (UP_BUTTON_PRESSED) {
      if (_edit_field_index > 0) {
        _edit_field_index--;
      }
    }
    // Left = Return to the idle screen
    if (LEFT_BUTTON_PRESSED) {
      _menu_state = MENU_STATE_1_IDLE;
    }
    // Right = Enter the selected menu
    if (RIGHT_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:
          _menu_state = MENU_STATE_3_DO_CLOCK_DIAGS;
          break;
        case 1:
          // Enter Set Time Menu
          load_editable_time_fields();
          _menu_state = MENU_STATE_4_DO_SET_TIME;
          break;

        case 2:
          // Enter Set Date Menu
          load_editable_time_fields();
          _menu_state = MENU_STATE_5_DO_SET_DATE;
          break;
        case 3:
          // Enter Set Wake & Sleep Menu
          load_editable_time_fields();
          _menu_state = MENU_STATE_6_DO_SET_SLEEP;
          break;
        case 4:
          _menu_state = MENU_STATE_7_DO_MAN_VALVES;
          break;
        case 5:
          _menu_state = MENU_STATE_8_DO_MAN_PUMP;
          break;
        case 6:
          _menu_state = MENU_STATE_9_DO_MAN_SETPOINTS;
          break;
        case 7:
          _menu_state = MENU_STATE_10_DO_SET_WIFI;
          break;
      }
      _edit_field_index = 0;  // Start with the first selection on the next screen
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_3_clock_diags_state() {
    print_menu_header("-----Diagnostics-----");

    _canvas->println();
    _canvas->println("DIAG TBD");

    // Left = return to prior menu state
    if (LEFT_BUTTON_PRESSED) {
      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_4_set_time_state() {
    // Present the menu editing time.
    // There are two fields to edit, HOUR (index 0) and MINUTES (index 1)
    print_menu_header("------Edit Time------");

    // Draw the items to be edited in double size
    _canvas->setTextSize(2);
    _canvas->println(F("   HH:MM"));

    _canvas->print(F("   "));
    print_zero_padded_two_digit_uint(_edit_rtc_hours, (_edit_field_index == 0));
    _canvas->print(F(":"));
    print_zero_padded_two_digit_uint(_edit_rtc_minutes, (_edit_field_index == 1));
    _canvas->setTextSize(1);  // Normal size

    // Down = Decrement the edited time parameter
    if (DOWN_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:
          // Editing Hour, decrement down to 0.
          if (_edit_rtc_hours > 0) {
            _edit_rtc_hours--;
          }
          break;
        case 1:
          // Editing minutes, decrementing down to 0.
          if (_edit_rtc_minutes > 0) {
            _edit_rtc_minutes--;
          }
          break;
      }
    }
    // Up = Increment the edited time parameter
    if (UP_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:
          // Editing Hour, increment to to 23.
          if (_edit_rtc_hours < 23) {
            _edit_rtc_hours++;
          }
          break;
        case 1:
          // Editing minutes, increment up to 59.
          if (_edit_rtc_minutes < 59) {
            _edit_rtc_minutes++;
          }
          break;
      }
    }
    // Left = Move through fields.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      if (_edit_field_index == 0) {
        return MENU_STATE_2_SELECT_MENU;
      } else {
        _edit_field_index--;
      }
    }
    // Right = Alternate between fields
    if (RIGHT_BUTTON_PRESSED) {
      if (_edit_field_index < 1) {
        _edit_field_index++;
      }
    }
    // Enter = set the time
    if (ENTER_BUTTON_PRESSED) {
      // Set the time using the clock manager
      _clock_man->Set_Time(_edit_rtc_seconds, _edit_rtc_minutes, _edit_rtc_hours, _edit_rtc_weekday, _edit_rtc_date, _edit_rtc_month, _edit_rtc_year);
      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_5_set_date_state() {
    // Present the menu editing time.
    // There are three fields to edit, YEAR (index 0), MONTH (index 1) and Day (index 2)
    print_menu_header("------Edit Date------");

    _canvas->setTextSize(2);  // Double size
    _canvas->println(F("YYYY-MM-DD"));

    print_zero_padded_four_digit_uint(_edit_rtc_year, (_edit_field_index == 0));
    _canvas->print(F("-"));
    print_zero_padded_two_digit_uint(_edit_rtc_month, (_edit_field_index == 1));
    _canvas->print(F("-"));
    print_zero_padded_two_digit_uint(_edit_rtc_date, (_edit_field_index == 2));
    _canvas->setTextSize(1);  // Normal size

    // Down = Decrement the edited time parameter
    if (DOWN_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:  // Editing Year, decrement down to 2023.
          if (_edit_rtc_year > 2023) {
            _edit_rtc_year--;
          }
          break;
        case 1:  // Editing month, decrementing down to 0.
          if (_edit_rtc_month > 0) {
            _edit_rtc_month--;
          }
          break;
        case 2:  // Editing day, decrementing down to 1.
          if (_edit_rtc_date > 1) {
            _edit_rtc_date--;
          }
          break;
      }
    }
    // Up = Increment the edited time parameter
    if (UP_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:  // Editing year, increment to to 2050.
          if (_edit_rtc_year < 2050) {
            _edit_rtc_year++;
          }
          break;
        case 1:  // Editing month, increment up to 11.
          if (_edit_rtc_month < 12) {
            _edit_rtc_month++;
          }
          break;
        case 2:  // Editing day, increment up to 31
          if (_edit_rtc_date < 31) {
            _edit_rtc_date++;
          }
          break;
      }
    }
    // Left = Move through fields.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      if (_edit_field_index == 0) {
        return MENU_STATE_2_SELECT_MENU;
      } else {
        _edit_field_index--;
      }
    }
    // Right = Alternate between fields
    if (RIGHT_BUTTON_PRESSED) {
      if (_edit_field_index < 2) {
        _edit_field_index++;
      } else {
        _edit_field_index = 0;
      }
    }
    // Enter = set the time
    if (ENTER_BUTTON_PRESSED) {
      // Set the time using the clock manager
      _clock_man->Set_Time(_edit_rtc_seconds, _edit_rtc_minutes, _edit_rtc_hours, _edit_rtc_weekday, _edit_rtc_date, _edit_rtc_month, _edit_rtc_year);
      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_6_set_sleep_state() {
    // Present the menu editing sleep time periods.
    // There are four fields to edit, Wake HOUR (index 0), Wake MIN (index 1), Sleep HOUR (index 2), Sleep MIN index 3)
    print_menu_header("-----Sleep Time------");

    // Draw the items to be edited in double size
    _canvas->setTextSize(2);

    _canvas->println(F("WAKE  SLEEP"));
    _canvas->println(F("HH:MM HH:MM"));
    //_canvas->print(F("  "));
    print_zero_padded_two_digit_uint(_edit_wake_hour, (_edit_field_index == 0));
    _canvas->print(F(":"));
    print_zero_padded_two_digit_uint(_edit_wake_min, (_edit_field_index == 1));
    _canvas->print(F(" "));
    print_zero_padded_two_digit_uint(_edit_sleep_hour, (_edit_field_index == 2));
    _canvas->print(F(":"));
    print_zero_padded_two_digit_uint(_edit_sleep_min, (_edit_field_index == 3));

    // Down = Decrement the edited time parameter
    if (DOWN_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:
          // Editing Wake Hour, decrement down to 0.
          if (_edit_wake_hour > 0) {
            _edit_wake_hour--;
          }
          break;
        case 1:
          // Editing Wake Min, decrementing down to 0.
          if (_edit_wake_min > 0) {
            _edit_wake_min--;
          }
          break;
        case 2:
          // Editing Sleep Hour, decrement down to 0.
          if (_edit_sleep_hour > 0) {
            _edit_sleep_hour--;
          }
          break;
        case 3:
          // Editing Sleep Min, decrementing down to 0.
          if (_edit_sleep_min > 0) {
            _edit_sleep_min--;
          }
          break;
      }
    }
    // Up = Increment the edited time parameter
    if (UP_BUTTON_PRESSED) {
      switch (_edit_field_index) {
        case 0:
          // Editing Wake Hour, increment to to 23.
          if (_edit_wake_hour < 23) {
            _edit_wake_hour++;
          }
          break;
        case 1:
          // Editing Wake Min, increment up to 59.
          if (_edit_wake_min < 59) {
            _edit_wake_min++;
          }
          break;
        case 2:
          // Editing Sleep Hour, increment to to 23.
          if (_edit_sleep_hour < 23) {
            _edit_sleep_hour++;
          }
          break;
        case 3:
          // Editing Sleep Min, increment up to 59.
          if (_edit_sleep_min < 59) {
            _edit_sleep_min++;
          }
          break;
      }
    }
    // Left = Move through fields.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      if (_edit_field_index == 0) {
        return MENU_STATE_2_SELECT_MENU;
      } else {
        _edit_field_index--;
      }
    }
    // Right = Alternate between fields
    if (RIGHT_BUTTON_PRESSED) {
      if (_edit_field_index < 3) {
        _edit_field_index++;
      }
    }
    // Enter = set the new wake & sleep times
    if (ENTER_BUTTON_PRESSED) {
      // Apply the edited values
      backup_settings.wake_hour = _edit_wake_hour;
      backup_settings.wake_min = _edit_wake_min;
      backup_settings.sleep_hour = _edit_sleep_hour;
      backup_settings.sleep_min = _edit_sleep_min;

      // Save the values in non-volatile memory
      save_preferences();

      // Apply the sleep times
      apply_clock_sleep_settings();

      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_7_man_valves_state() {

    // Switch to valve override mode while manually controlling
    Set_Operating_Mode(UIManager::OPERATING_MODE_VALVE_OVERRIDE);

    // Present the valve edit screen
    // There are three fields to edit, HOUR Column(index 0), MIN 10s column(index 1), MIN 1s column(index 2)
    print_menu_header("----MANUAL VALVES----");

    // Black out the area above the columns for up arrows
    _canvas->fillRect(18, 10,  // x, y
                      90,      // Width
                      8,       // height
                      BLACK);

    // Black out the area above the columns for up arrows
    _canvas->fillRect(18, 86,  // x, y
                      90,      // Width
                      8,       // height
                      BLACK);

    // Draw the up and down arrows above and below the column selected
    switch (_edit_field_index) {
      case 0:
        // Adjusting the HOUR column
        _canvas->setCursor(18 + 5, 10);  // Draw above the hour column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(18 + 5, 88);  // Draw below the hour column
        _canvas->write(0x1F);            // Down arrow
        break;

      case 1:
        // Adjusting the MIN 10s column
        _canvas->setCursor(54 + 5, 10);  // Draw above the min 10s column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(54 + 5, 88);  // Draw below the min 10s column
        _canvas->write(0x1F);            // Down arrow
        break;

      case 2:
        // Adjusting the MIN 1s column
        _canvas->setCursor(90 + 5, 10);  // Draw above the min 1s column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(90 + 5, 88);  // Draw below the min 1s column
        _canvas->write(0x1F);            // Down arrow
        break;
    }

    // Render the columns graphically
    draw_column_symbol(18,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_hour->Get_Elevation_Reading_MM(),
                       _column_manager_hour->Get_Setpoint_Upper_Limit());

    draw_column_symbol(54,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_min_10s->Get_Elevation_Reading_MM(),
                       _column_manager_min_10s->Get_Setpoint_Upper_Limit());

    draw_column_symbol(90,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_min_1s->Get_Elevation_Reading_MM(),
                       _column_manager_min_1s->Get_Setpoint_Upper_Limit());

    // Draw the elevation readings right under the column
    _canvas->setCursor(18, 100);
    _canvas->printf("%3d", _column_manager_hour->Get_Elevation_Reading_MM());
    _canvas->setCursor(54, 100);
    _canvas->printf("%3d", _column_manager_min_10s->Get_Elevation_Reading_MM());
    _canvas->setCursor(90, 100);
    _canvas->printf("%3d", _column_manager_min_1s->Get_Elevation_Reading_MM());

    // Draw instructions at the bottom
    _canvas->setCursor(0, 120);
    _canvas->print(F(" Hold "));
    _canvas->write(0x1E);  // Up arrow
    _canvas->print(F(" or "));
    _canvas->write(0x1F);  // Down arrow
    _canvas->println(F(" to flow. "));

    // Left = Move through columns.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      if (_edit_field_index == 0) {

        // Switch back to clock mode
        Set_Operating_Mode(UIManager::OPERATING_MODE_CLOCK);

        return MENU_STATE_2_SELECT_MENU;
      } else {
        _edit_field_index--;
      }
    }
    // Right = Alternate between fields
    if (RIGHT_BUTTON_PRESSED) {
      if (_edit_field_index < 2) {
        _edit_field_index++;
      }
    }

    // Down = Activate the FILL valve for the hour column
    if (DOWN_BUTTON_ACTIVE) {
      // Request a manual drain for 150msec
      switch (_edit_field_index) {
        case 0:
          _column_manager_hour->Manual_Drain(150);
          break;
        case 1:
          _column_manager_min_10s->Manual_Drain(150);
          break;
        case 2:
          _column_manager_min_1s->Manual_Drain(150);
          break;
      }
    }
    // Up = Activate the DRAIN valve for the hour column
    if (UP_BUTTON_ACTIVE) {
      // Request a manual fill for 150msec
      switch (_edit_field_index) {
        case 0:
          _column_manager_hour->Manual_Fill(150);
          break;
        case 1:
          _column_manager_min_10s->Manual_Fill(150);
          break;
        case 2:
          _column_manager_min_1s->Manual_Fill(150);
          break;
      }
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_8_man_pump_state() {

    // Present the valve hour edit screen
    // There are three fields to edit, HOUR Column(index 0), MIN 10s column(index 1), MIN 1s column(index 2)
    print_menu_header("-----MANUAL PUMP-----");

    bool lower_level_sensor_wet = _tank->Is_Feed_Tank_Above_Low_Mark();
    bool upper_level_sensor_wet = _tank->Is_Feed_Tank_Above_High_Mark();
    bool pump_running = _tank->Is_Pump_Active();

    // Draw the tank schematic
    draw_tank_schematic(30, 30,  // Tank upper left coordinate
                        60, 30,  // Tank width and height
                        lower_level_sensor_wet,
                        upper_level_sensor_wet,
                        pump_running);

    // Draw instructions at the bottom
    _canvas->setCursor(0, 120);
    _canvas->print(F(" Hold "));
    _canvas->write(0x1E);  // Up arrow
    _canvas->print(F(" to run pumop. "));

    // Left = Move through columns.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      return MENU_STATE_2_SELECT_MENU;
    }

    // Up = Activate the DRAIN valve for the hour column
    if (UP_BUTTON_ACTIVE) {
      // Request a manual fill for 150msec
      _tank->Manual_Fill(150);
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_9_man_setpoints_state() {

    // Switch to valve override mode while manually controlling
    Set_Operating_Mode(UIManager::OPERATING_MODE_STATIC_OVERRIDE);

    // Present the column setpoint edit screen
    // There are three fields to edit, HOUR Column(index 0), MIN 10s column(index 1), MIN 1s column(index 2)
    print_menu_header("--MANUAL SETPOINTS--");

    // Black out the area above the columns for up arrows
    _canvas->fillRect(18, 10,  // x, y
                      90,      // Width
                      8,       // height
                      BLACK);

    // Black out the area above the columns for up arrows
    _canvas->fillRect(18, 86,  // x, y
                      90,      // Width
                      8,       // height
                      BLACK);

    // Draw the up and down arrows above and below the column selected
    switch (_edit_field_index) {
      case 0:
        // Adjusting the HOUR column
        _canvas->setCursor(18 + 5, 10);  // Draw above the hour column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(18 + 5, 88);  // Draw below the hour column
        _canvas->write(0x1F);            // Down arrow
        break;

      case 1:
        // Adjusting the MIN 10s column
        _canvas->setCursor(54 + 5, 10);  // Draw above the min 10s column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(54 + 5, 88);  // Draw below the min 10s column
        _canvas->write(0x1F);            // Down arrow
        break;

      case 2:
        // Adjusting the MIN 1s column
        _canvas->setCursor(90 + 5, 10);  // Draw above the min 1s column
        _canvas->write(0x1E);            // Up arrow
        _canvas->setCursor(90 + 5, 88);  // Draw below the min 1s column
        _canvas->write(0x1F);            // Down arrow
        break;
    }

    // Render the columns graphically
    draw_column_symbol(18,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_hour->Get_Elevation_Reading_MM(),
                       _column_manager_hour->Get_Setpoint_Upper_Limit());

    draw_column_symbol(54,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_min_10s->Get_Elevation_Reading_MM(),
                       _column_manager_min_10s->Get_Setpoint_Upper_Limit());

    draw_column_symbol(90,                     // x0,
                       22,                     // y0,
                       COLUMN_GRAPHIC_WIDTH,   // width,
                       COLUMN_GRAPHIC_HEIGHT,  // height,
                       _column_manager_min_1s->Get_Elevation_Reading_MM(),
                       _column_manager_min_1s->Get_Setpoint_Upper_Limit());

    // Draw the elevation setpoint under the column
    _canvas->setCursor(18, 100);
    _canvas->printf("%3d", Get_Hour_Column_Override_Setpoint());
    _canvas->setCursor(54, 100);
    _canvas->printf("%3d", Get_Min_10s_Column_Override_Setpoint());
    _canvas->setCursor(90, 100);
    _canvas->printf("%3d", Get_Min_1s_Column_Override_Setpoint());

    // Draw the current elevation readings
    _canvas->setCursor(18, 110);
    _canvas->printf("%3d", _column_manager_hour->Get_Elevation_Reading_MM());
    _canvas->setCursor(54, 110);
    _canvas->printf("%3d", _column_manager_min_10s->Get_Elevation_Reading_MM());
    _canvas->setCursor(90, 110);
    _canvas->printf("%3d", _column_manager_min_1s->Get_Elevation_Reading_MM());

    // Draw instructions at the bottom
    _canvas->setCursor(0, 120);
    _canvas->print(F("Hold "));
    _canvas->write(0x1E);  // Up arrow
    _canvas->print(F(" or "));
    _canvas->write(0x1F);  // Down arrow
    _canvas->println(F(" for setp."));

    // Left = Move through columns.  If all the way left then abandon edits and return to prior menu.
    if (LEFT_BUTTON_PRESSED) {
      if (_edit_field_index == 0) {

        // Switch back to clock mode
        Set_Operating_Mode(UIManager::OPERATING_MODE_CLOCK);

        return MENU_STATE_2_SELECT_MENU;
      } else {
        _edit_field_index--;
      }
    }
    // Right = Alternate between fields
    if (RIGHT_BUTTON_PRESSED) {
      if (_edit_field_index < 2) {
        _edit_field_index++;
      }
    }

    // Down = Activate the FILL valve for the hour column
    if (DOWN_BUTTON_ACTIVE) {
      // Request a manual drain for 150msec
      switch (_edit_field_index) {
        case 0:
          //adjust hour setpoint higher to limit
          Set_Hour_Column_Override_Setpoint(increment_setpoint(Get_Hour_Column_Override_Setpoint(),
                                                               2,
                                                               _column_manager_hour->Get_Setpoint_Upper_Limit()));
          break;
        case 1:
          //adjust min 10s setpoint higher to limit
          Set_Min_10s_Column_Override_Setpoint(increment_setpoint(Get_Min_10s_Column_Override_Setpoint(),
                                                                  2,
                                                                  _column_manager_min_10s->Get_Setpoint_Upper_Limit()));
          break;
        case 2:
          //adjust min 1s setpoint higher to limit
          Set_Min_1s_Column_Override_Setpoint(increment_setpoint(Get_Min_1s_Column_Override_Setpoint(),
                                                                 2,
                                                                 _column_manager_min_1s->Get_Setpoint_Upper_Limit()));

          break;
      }
    }
    // Up = Activate the DRAIN valve for the hour column
    if (UP_BUTTON_ACTIVE) {
      // Request a manual fill for 150msec
      switch (_edit_field_index) {
        case 0:
          //adjust hour setpoint lower to limit
          Set_Hour_Column_Override_Setpoint(decrement_setpoint(Get_Hour_Column_Override_Setpoint(),
                                                               2,
                                                               _column_manager_hour->Get_Setpoint_Lower_Limit()));
          break;

        case 1:
          //adjust min 10s setpoint lower to limit break;
          Set_Min_10s_Column_Override_Setpoint(decrement_setpoint(Get_Min_10s_Column_Override_Setpoint(),
                                                                  2,
                                                                  _column_manager_min_10s->Get_Setpoint_Lower_Limit()));
          break;
        case 2:
          //adjust min 1s setpoint lower to limit break;
          Set_Min_1s_Column_Override_Setpoint(decrement_setpoint(Get_Min_1s_Column_Override_Setpoint(),
                                                                 2,
                                                                 _column_manager_min_1s->Get_Setpoint_Lower_Limit()));
          break;
      }
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  MENU_STATE_T do_menu_10_set_wifi_state() {
    // Present the wifi settings edit screen
    print_menu_header("----WIFI SETTINGS----");

    if (LEFT_BUTTON_PRESSED) {
      return MENU_STATE_2_SELECT_MENU;
    }

    // Default is to stay in the current state
    return _menu_state;
  }


  void set_tank_regulator_enable(bool enable) {
    _tank->Set_Regulator_Enable(enable);
  }


  void set_hour_column_regulator_enable(bool enable) {
    _column_manager_hour->Set_Regulator_Enable(enable);
  }


  void set_hour_column_override_setpoint_enable(bool enable) {
    _hour_override_setpoint_enable = enable;
  }


  void set_min_10s_column_regulator_enable(bool enable) {
    //_min_10s_regulator_enable = enable;
    _column_manager_min_10s->Set_Regulator_Enable(enable);
  }


  void set_min_10s_column_override_setpoint_enable(bool enable) {
    _min_10s_override_setpoint_enable = enable;
  }


  void set_min_1s_column_regulator_enable(bool enable) {
    _column_manager_min_1s->Set_Regulator_Enable(enable);
  }


  void set_min_1s_column_override_setpoint_enable(bool enable) {
    _min_1s_override_setpoint_enable = enable;
  }


  uint16_t increment_setpoint(uint16_t cur_setpoint, uint16_t inc_value, uint16_t setpoint_max) {
    int16_t new_setpoint = cur_setpoint + inc_value;
    if (new_setpoint >= setpoint_max) {
      return setpoint_max;
    }
    return new_setpoint;
  }


  uint16_t decrement_setpoint(uint16_t cur_setpoint, uint16_t dec_value, uint16_t setpoint_min) {
    int16_t new_setpoint = cur_setpoint - dec_value;

    if (new_setpoint <= setpoint_min) {
      return setpoint_min;
    }
    return new_setpoint;
  }


  bool scan_left_button_input() {
    bool active = !_io_expander->digitalRead(SC1509_PIN_KEY_4);
    return active;
  }


  bool scan_right_button_input() {
    bool active = !_io_expander->digitalRead(SC1509_PIN_KEY_5);
    return active;
  }


  bool scan_up_button_input() {
    bool active = !_io_expander->digitalRead(SC1509_PIN_KEY_3);
    return active;
  }


  bool scan_down_button_input() {
    bool active = !_io_expander->digitalRead(SC1509_PIN_KEY_2);
    return active;
  }


  bool scan_enter_button_input() {
    bool active = !_io_expander->digitalRead(SC1509_PIN_KEY_1);
    return active;
  }


  void detect_button_activity() {
    // Update the button inputs to look for changes since the last menu update
    cur_button_status.left_button_active = scan_left_button_input();
    cur_button_status.right_button_active = scan_right_button_input();
    cur_button_status.up_button_active = scan_up_button_input();
    cur_button_status.down_button_active = scan_down_button_input();
    cur_button_status.enter_button_active = scan_enter_button_input();
  }


  void debounce_buttons() {
    // Schedule button input debounce
    if (_menu_button_debounce_period_elapsed < MENU_BUTTON_DEBOUNCE_PERIOD_MS) {
      return;
    }
    _menu_button_debounce_period_elapsed = 0;

    //TODO: Add debounce filtering in fuure
  }


  // Update editable time fields with latest clock and settings.
  void load_editable_time_fields() {

    // Grab the current date/time to seed the date/time settings
    _edit_rtc_year = _clock_man->Get_Year();
    _edit_rtc_month = _clock_man->Get_Month();
    _edit_rtc_date = _clock_man->Get_Day();
    _edit_rtc_hours = _clock_man->Get_Hour();
    _edit_rtc_minutes = _clock_man->Get_Minute();
    _edit_rtc_seconds = _clock_man->Get_Second();

    // Grab the sleep setting restored from non-volatile memory
    _edit_wake_hour = backup_settings.wake_hour;
    _edit_wake_min = backup_settings.wake_min;
    _edit_sleep_hour = backup_settings.sleep_hour;
    _edit_sleep_min = backup_settings.sleep_min;
  }


  // After the UI has adjusted the sleed parameters, we must push
  // the new values to the clock manager to utilize.
  void apply_clock_sleep_settings() {

    _clock_man->Set_Wake_Hour(backup_settings.wake_hour);
    _clock_man->Set_Wake_Min(backup_settings.wake_min);
    _clock_man->Set_Sleep_Hour(backup_settings.sleep_hour);
    _clock_man->Set_Sleep_Min(backup_settings.sleep_min);
  }


  // Draw a standard menu header with the specific menu name.
  void print_menu_header(const char *text) {
    _canvas->setTextColor(TEXT_COLOR_TITLE, BLACK);  // Default is white text over black background
    _canvas->println(text);
    _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Default is white text over black background
    _canvas->println();
  }


  // Standardized menu parameter printer with options for inversion
  void print_parameter(const char *text, bool linefeed, bool inverted) {
    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, TEXT_COLOR_HIGHLIGHT);
    }

    _canvas->print(text);
    if (linefeed) {
      _canvas->println();
    }

    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Uninvert
    }
  }


  // Standard print for a two digit parameter with zero padding.  Used for fields
  // like hours or minutes.
  void print_zero_padded_two_digit_uint(uint16_t value, bool inverted) {
    char _txt_buffer[32];

    sprintf(_txt_buffer, "%02d", value);
    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, TEXT_COLOR_HIGHLIGHT);
    }
    _canvas->print(_txt_buffer);

    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Uninvert
    }
  }


  // Standard print for a four digit parameter with zero padding.  Used for fields
  // like the year.
  void print_zero_padded_four_digit_uint(uint16_t value, bool inverted) {
    char _txt_buffer[32];

    sprintf(_txt_buffer, "%04d", value);
    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, TEXT_COLOR_HIGHLIGHT);
    }
    _canvas->print(_txt_buffer);

    if (inverted) {
      _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Uninvert
    }
  }


  // Draw a graphical symbol for a water column with a variable level of water
  void draw_column_symbol(uint8_t col_x0, uint8_t col_y0, uint8_t col_width, uint8_t col_height, uint16_t elevation_mm, uint16_t upper_elevation) {

    // Calculate how much of the column needs to be filled with fluid in percentage.
    // An elevation reading close to the upper elevation limit is an empty column.
    // A low elevation reading means the float is at the top thus a full column.
    // This is why we subtract the elevation ration from 100 to invert.
    float fill_percentage = 1.0 - (((float)elevation_mm * 1.0) / (float)upper_elevation); /* % = [0.0 - 1.0] */

    // Draw the column
    draw_water_vessel(col_x0, col_y0, col_width, col_height, fill_percentage, WHITE, CYAN);
  }


  // Draw a graphical symbol for a water tank with level sensor status, water level and pump activity
  void draw_tank_schematic(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, bool lower_sensed, bool upper_sensed, bool pump_active) {
    static uint8_t pump_anim_phase = 0;

    // Draw the tank and contents
    if (upper_sensed) {
      // Full tank to %75 full
      draw_water_vessel(x0, y0, width, height, 0.75f, WHITE, CYAN);
    } else if (lower_sensed) {
      // Full tank to %25 full
      draw_water_vessel(x0, y0, width, height, 0.25f, WHITE, CYAN);
    } else {
      // Tank is completely empty
      draw_water_vessel(x0, y0, width, height, 0.0f, WHITE, CYAN);
    }

    _canvas->setTextColor(RED, BLACK);  // Switch to red for level sensor status

    // Draw the upper sensor status to the left of the tank symbol
    _canvas->setCursor(x0 - 8, (y0 + height) - (0.75f * height));  // Cursor to 75% on left of frame
    if (upper_sensed) {
      _canvas->print("H");
    } else {
      _canvas->print("L");
    }

    // Draw the lower sensor status to the left of the tank symbol
    _canvas->setCursor(x0 - 8, (y0 + height) - (0.25f * height));  // Cursor to 25% on left of frame
    if (lower_sensed) {
      _canvas->print("H");
    } else {
      _canvas->print("L");
    }

    _canvas->setTextColor(TEXT_COLOR_BASE, BLACK);  // Return to default text color

    // Draw pipes from the pump to the upper tank
    _canvas->fillRect(x0 + width, y0 + height - 5, 10, 4, CYAN);       // horizontal pipe out of upper tank
    _canvas->fillRect(x0 + width + 10, y0 + height - 5, 4, 15, CYAN);  // vertical pipe down to pump

    // Draw pipe below pump
    _canvas->fillRect(x0 + width + 10, y0 + height + 30, 4, 15, CYAN);  // vertical pipe down to pump

    // Draw pump activity by alternating between an empty and filled pump circle
    if (pump_active) {
      if (pump_anim_phase) {
        pump_anim_phase = 0;
      } else {
        pump_anim_phase = 1;
        _canvas->fillCircle(x0 + width + 12, y0 + height + 20, 10, RED);
      }
    }

    // Draw the pump outline offset to the right and below the upper tank
    _canvas->drawCircle(x0 + width + 12, y0 + height + 20, 10, GREEN);
  }


  // Draw a generic water vessel with a specificed percentage of water filled
  void draw_water_vessel(uint16_t x, uint16_t y,           // Upper left coordinate of vessel
                         uint16_t width, uint16_t height,  // width and height of vessel
                         float percent_filled,             // 0.0 to 1.0
                         uint16_t frame_color,
                         uint16_t water_color) {
    // Draw the vessel outer frame
    _canvas->drawRect(x, y, width, height, frame_color);

    // Scale the vessel height with the fill_percentage to compute how high to draw the water
    float water_height = height * percent_filled;

    // Draw the water at the bottom of the vessel with the scaled height
    _canvas->fillRect(x + 1,                     /* 1 pixel in on left to not draw over left frame */
                      y + height - water_height, /* Shift water to bottom of column */
                      width - 1,                 /* 1 pixel in on right to not draw over right frame */
                      water_height,
                      water_color);
  }


  // Draw a horizontal progress bar that fills from the left to the right in percentage
  void draw_progress_bar(uint16_t x, uint16_t y,           // Upper left coordinate of vessel
                         uint16_t width, uint16_t height,  // width and height of vessel
                         float percent_filled,             // 0.0 to 1.0
                         uint16_t frame_color,
                         uint16_t bar_color) {
    // Draw the bar outer frame
    _canvas->drawRect(x, y, width, height, frame_color);

    // Scale the bar width by the fill_percentage to compute how far to draw the bar
    float bar_width = (width - 2) * percent_filled;

    // Draw the bar at the left of the frame with the scaled width
    _canvas->fillRect(x + 1,     /* 1 pixel in on left to not draw over left frame */
                      y + 1,     /* Shift water to bottom of column */
                      bar_width, /* 1 pixel in on right to not draw over right frame */
                      height - 2,
                      bar_color);
  }


  // Display a list of active faults using the fault description strings
  void display_faults() {
    for (int i = 0; i < FAULT_MAX_INDEX; i++) {
      if (FAULT_ACTIVE(i)) {
        _canvas->println(FAULT_STRING[i]);
      }
    }
  }
};

#endif
