////////////////////////////////////////////////////////////////////////
// calibration.cpp
// Calibration Source File
// by: Aistheta Gleason
////////////////////////////////////////////////////////////////////////
// Safety Note: Read the README!!! Keep Battery in well ventilated area
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Copyright and License
////////////////////////////////////////////////////////////////////////
// Copyright 2022, Aistheta (Adam) Gleason
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or 
// (at your option) any later version.
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
// You should have received a copy of the GNU General Public License
// (LICENSE) along with this program. If not, see 
// https://www.gnu.org/licenses/
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////
// Load Config
#include "config.h"
#ifdef CAL
// MPPT Library
#include "mppt.h"
// Timer 1 Library
#include <TimerOne.h>


////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////
// User measured battery and solar voltage
volatile double user_bat, user_sol;
// State Variable Definition
volatile CAL_STATES cur_cal_state;
// input byte variable
char inbyte;
// Input Byte Buffer
char inbytes[100];
// Input Byte Buffer Index
char inbyte_i;
// Temp string for sprintfs
char tempstr[100];
// Calibrating flag
bool calibrating;
// Battery Average
double avg_bat;
// Solar Average
double avg_sol;
// Number of MPPTs
int n_mppt;

////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// setup_calibration() function
// Setup for calibration
////////////////////////////////////////////////////////////////////////
void setup_calibration() {
  // Start Serial
  Serial.begin(115200);
  delay(1000);
  // Print Beggining Test
  Serial.println("-----------------------------------");
  Serial.println("Solar Charger Calibration and Self Test Report");
  Serial.println("Type Y and Press Enter to Begin");
  Serial.println("-----------------------------------");
  // set current state
  cur_cal_state = INIT_CAL;
  // Set inbyte to 0
  inbyte = 0;
  // Set inbyte index to 0
  inbyte_i = 0;
  // Set calibrating to 1
  calibrating = 1;
  // Init n_mppt to 0
  n_mppt = 0;
}

////////////////////////////////////////////////////////////////////////
// calibration_state_machine() function
// State machine for calibration
////////////////////////////////////////////////////////////////////////
void calibration_state_machine() {
  switch (cur_cal_state) {
    ////////////////////////////////////////////////////////////////////////
    // INIT_CAL State
    // waits for user to press enter to transition to MEAS state
    ////////////////////////////////////////////////////////////////////////
    case INIT_CAL:
      // Wait for enter key (new line)
      if (Serial.available() > 0) {
        inbyte = Serial.read();
        // echo character
        Serial.print(inbyte);
        // 0x0A = \n 0x59 = Y
        if (inbyte == 0x0A || inbyte == 0x59) {
          cur_cal_state = MEAS;
          break;
        }
      }
      break;
    ////////////////////////////////////////////////////////////////////////
    // MEAS State
    // Measures solar and battery voltage and reports to user
    ////////////////////////////////////////////////////////////////////////
    case MEAS:
      // Measure battery and solar voltages
      check_battery();
      check_solar();
      Serial.println("Measuring Battery and Solar Voltages");
      Serial.println("-----------------------------------");
      sprintf(tempstr, "Battery Voltage = %f V", v_battery);
      Serial.println(tempstr);
      sprintf(tempstr, "Solar Voltage = %f V", v_solar);
      Serial.println(tempstr);
      Serial.println("-----------------------------------");
      cur_cal_state = USER_BAT;
      Serial.println("Take a DMM, Measure the Battery Voltage, Type it here and press Enter:");
      Serial.println("**NOTE: Backspace doesn't work, make sure to type in perfectly or cycle power**");
      inbyte_i = 0;
      // Init battery average
      avg_bat = v_battery;
      break;
    ////////////////////////////////////////////////////////////////////////
    // USER_BAT State
    // Averages battery voltage and waits for user to input measured voltage
    ////////////////////////////////////////////////////////////////////////
    case USER_BAT:
      // Check battery
      check_battery();
      // Average battery voltage
      avg_bat += v_battery;
      avg_bat /= 2;
      // Read User Input
      if (Serial.available() > 0 && inbyte_i < 100) {
        // read character
        inbytes[inbyte_i] = Serial.read();
        // echo character
        Serial.write(inbytes[inbyte_i]);
        if (inbytes[inbyte_i++] == 0x0A) {
          // set newline to 0, maybe not necessary
          inbytes[inbyte_i - 1] = 0;
          // Convert inbytes to float, size inbyte_i
          user_bat = atof(inbytes);
          cur_cal_state = USER_SOL;
          Serial.println("-----------------------------------");
          Serial.println("Take a DMM, Measure the Solar Voltage, Type it here and press Enter:");
          Serial.println("**NOTE: Backspace doesn't work, make sure to type in perfectly or cycle power**");
          // Reset inbyte index
          inbyte_i = 0;
          // take fresh solar measurement
          check_solar();
          // Initialize solar average
          avg_sol = v_solar;
          break;
        }
      }
      break;
    ////////////////////////////////////////////////////////////////////////
    // USER_SOL State
    // Averages solar voltage and waits for user to input measured voltage
    ////////////////////////////////////////////////////////////////////////
    case USER_SOL:
      // Average Solar Voltage
      check_solar();
      avg_sol += v_solar;
      avg_sol /= 2;
      // Read User Input
      if (Serial.available() > 0 && inbyte_i < 100) {
        // read character
        inbytes[inbyte_i] = Serial.read();
        // echo character
        Serial.write(inbytes[inbyte_i]);
        // If pressed enter
        if (inbytes[inbyte_i++] == 0x0A) {
          // set newline to 0, maybe not necessary
          inbytes[inbyte_i - 1] = 0;
          // Convert inbytes to float, size inbyte_i
          user_sol = atof(inbytes);
          cur_cal_state = CALC;
          break;
        }
      }
      break;
    ////////////////////////////////////////////////////////////////////////
    // CALC State
    // Calculates errors and corrected VBAT and VSOL COEFs
    ////////////////////////////////////////////////////////////////////////
    case CALC:
      Serial.println("-----------------------------------");
      // Calculate and report measurement errors
      sprintf(tempstr, "Battery Voltage Measurement Error = %f percent", 100 * (ABS(user_bat - avg_bat) / (user_bat)));
      Serial.println(tempstr);
      sprintf(tempstr, "Solar Voltage Measurement Error = %f percent", 100 * (ABS(user_sol - avg_sol) / (user_sol)));
      Serial.println(tempstr);
      // Calculate and report correct coeffs for user to re-run with
      sprintf(tempstr, "#define VBAT_COEF (%e)", (user_bat / avg_bat)*VBAT_COEF);
      Serial.println(tempstr);
      sprintf(tempstr, "#define VSOL_COEF (%e)", (user_sol / avg_sol)*VSOL_COEF);
      Serial.println(tempstr);
      Serial.println("-----------------------------------");
      Serial.println("Replace #defines in config.h to match these above and recompile and download again");
      Serial.println("Recommend repeating this process until voltage measurement errors fall below 1%");
      Serial.println("Once fully satisified with errors comment out the \"#define CAL 1\" line in config.h");
      Serial.println("-----------------------------------");
      Serial.println("Calibration Complete");
      Serial.println("-----------------------------------");
      // Set state to DONE
      cur_cal_state = DONE_CAL;
      break;
    ////////////////////////////////////////////////////////////////////////
    // DONE_CAL State
    // Complete calibration, move on to self test
    ////////////////////////////////////////////////////////////////////////
    case DONE_CAL:
      // set main state to INIT_CHG (should be INIT anyways)
      cur_state = INIT_CHG;
      // End calibration
      calibrating = 0;
      // Printout Self Test
      Serial.println("Begging Self Test Report of Solar Charger");
      Serial.println("-----------------------------------");
      sprintf(tempstr, "Running %d Number of Tests", N_MPPT);
      Serial.println(tempstr);
      Serial.println("Values are CSV as follows:");
      Serial.println("v_battery, v_solar, integral_avg, p_cur, duty_cycle, time");
      break;

  }
}
// ifdef CAL
#endif
