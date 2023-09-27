/*
 * Aqua Clock calibration settings
 *
 * Captures the calibration tables used to translate a raw column range sensor reading into
 * corrected range values.
 *
 * Currently the linearization table contain unity values (output = input).
 * To calibrate a column follow these steps per column.
 *   Hour column calibration:
 *   1. Connect the serial monitor to the ESP32 USB, open the console.
 *   2. On the UI, enter the "MAN SETPOINT" menu item.
 *   3. Select the first column for Hour.
 *   4. Adjust the Up or Down buttons till the hour float is on the 1 mark.
 *   5. Record the raw elevation readino on the UI (lower 3 digit number) and enter
 *      this value into the first value in hour_col_digit_elevations[].
 *   6. Adjust the Up or Down buttons till the hour float is on the 2 mark.
 *   7. Record this raw reading into the second entry in hour_col_digit_elevations[].
 *   8. Repeat this process for each hour breakpoint.
 *   9. Save the adjusted contents of this calibration.h.  Recompile and upload to test.
 *   10. To test the hour column, adjust the time hour via the menu and observe the float.
 *
 * For the minutes 10s and 1s columns repeat the above process and save into the
 * min_10s_col_digit_elevations[] and min_1s_col_digit_elevations[] arrays.
 *
 * @author Joe Mohos
 * Contact: jmohos@yahoo.com
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H


//Range Sensor Calibration Interpolation tables
// x axis is the raw sensor distance measured in mm
// y axis is the actual physical distance measured in mm
const int NUM_RANGE_CAL_BREAKS = 7;
double hour_range_cal_x_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };  // Raw sensor inputs
double hour_range_cal_y_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };

double min_10s_range_cal_x_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };
double min_10s_range_cal_y_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };

double min_1s_range_cal_x_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };
double min_1s_range_cal_y_values[NUM_RANGE_CAL_BREAKS] = { 0, 50, 100, 150, 200, 250, 305 };


// Time to column elevation target tables
// Each breakpoint for the tables represents the time input (hour, min 10s, min 1s digits).
// Each aray entry at a breakpoint represents the matching range sensor elevation reading.
// Example, when the hour reaches 7 the 7th entry in hour_col_digit_elevations is what will be used as the target.
const uint16_t NUM_HOUR_STEPS = 12;
uint16_t hour_col_digit_elevations[NUM_HOUR_STEPS] = { 302, 282, 259, 239, 219, 199, 179, 157, 137, 117, 94, 56 };
//                                             Hour+1=   1    2    3    4    5    6    7    8    9   10   11   12

const uint16_t NUM_MIN_10S_STEPS = 6;
uint16_t min_10s_col_digit_elevations[NUM_MIN_10S_STEPS] = { 298, 248, 205, 165, 125, 60 };
//                                                             0    1    2    3    4    5

const uint16_t NUM_MIN_1S_STEPS = 10;
uint16_t min_1s_col_digit_elevations[NUM_MIN_1S_STEPS] = { 303, 280, 255, 224, 195, 165, 147, 125, 90, 50 };
//                                                           0    1    2    3    4    5    6    7   8   9

#endif
