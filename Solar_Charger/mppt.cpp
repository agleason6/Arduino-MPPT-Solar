////////////////////////////////////////////////////////////////////////
// mppt.cpp
// MPPT Source File
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
// MPPT Library
#include "mppt.h"
// Timer 1 Library
#include <TimerOne.h>
// Config Library
#include "config.h"


////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////
// State Variable Definition
volatile STATES cur_state;
// Duty Cycle Variable
volatile unsigned char duty_cycle;
// Solar and Battery Voltage Variables
volatile double v_solar, v_battery;
// Inductor Current and Previous Voltage Variables
volatile double vl_cur, vl_prev;
// MPPT Power Tracking Variables (for slopes)
volatile double p_cur, p_prev;
// Integral Variable
volatile long int integral;
// Time Tracking Variables (for dt integration)
volatile unsigned long int t_cur, t_prev;
// Number of Integrations
volatile unsigned char num_integrals;
// Average Integral Value (over NUM_INT integrals)
volatile long int integral_avg;
// PWM Count Variable
volatile unsigned char pwm_count;
// New Integral Flag, Timer On Flag, and Duty Cycle Increase Flag
volatile bool new_integral, timer_on, duty_inc;


////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////
// Built in reset function
void(* resetFunc) (void) = 0;

////////////////////////////////////////////////////////////////////////
// check_battery() function
// Checks the current battery level
// Set's state to done if battery level reaches or excedes charge level
////////////////////////////////////////////////////////////////////////
void check_battery() {
  // Measure Battery Voltage
  v_battery = VBAT_MEAS;
  // If Battery Charged
  if (v_battery >= VCHARGE) {
    timer_on = 0;
    cur_state = DONE_CHG;
  }
}

////////////////////////////////////////////////////////////////////////
// check_solar() function
// Reads the solar panel voltage
////////////////////////////////////////////////////////////////////////
void check_solar() {
  // Measure Solar Voltage
  v_solar = VSOL_MEAS;
}

////////////////////////////////////////////////////////////////////////
// PWM Handler Function
// Runs off the base timer frequency, which is 100 times faster
// PWM increments in by +- 1%
// Set's state to INTEGRATE when PWM signal is high
// Set's state to MPPT when PWM signal is low
////////////////////////////////////////////////////////////////////////
void pwm_handler() {
  // If Timer is ON
  if (timer_on) {
    // If PWM Counter less than Duty Cycle (%)
    if (++pwm_count <= duty_cycle) {
      // Set State to INTEGRATE
      cur_state = INTEGRATE;
      // If PWM Counter greater than Duty Cycle and less than 100
    } else if ((pwm_count > duty_cycle) && (pwm_count < 100)) {
      // Set State MPPT
      cur_state = MPPT;
      // If PWM Counter Overflows, reset to 0
    } else if (pwm_count >= 100) pwm_count = 0;
  }
}

////////////////////////////////////////////////////////////////////////
// init_charger()
// Initializes Charger after main setup
////////////////////////////////////////////////////////////////////////
void init_charger() {
  // Reset PWM count
  pwm_count = 0;
  // Reset Number of Integrals
  num_integrals = 0;
  // Reset Integral
  integral = 0;
  // Set new_integral to 0 (Algorithm starts in INTEGRATE after forced init and timer handler called)
  new_integral = 0;
  // Reset Integral Average
  integral_avg = 0;
  // Set Duty Cycle Increase Flag
  // Init to 1, because p_prev = 0 initially, it powers up (increases from 0) so assume increase at first
  duty_inc = 1;
  // Zero Out Power Tracking Variables (will read new cur on first and assume positive)
  p_prev = p_cur = 0;
  // Check Battery Level (Battery Only, Charger Not Running Yet)
  check_battery();
  // Check Solar Level
  check_solar();
  // Set VL prev to start integral
  vl_prev = VL_MEAS;
  // Set Initial Duty Cycle (Vsol*D = Vbat => D = Vbat/Vsol)
  // Will target current battery level then MPPT will nagivate around that
  duty_cycle = (char) (100 * ((double) v_battery / (double) v_solar));
  // Read Current Time, Set Both Previous and Current
  t_prev = t_cur = micros();
  // Set First State to INTEGRATE
  cur_state = INTEGRATE;
  // Turn on Timer
  timer_on = 1;
}

////////////////////////////////////////////////////////////////////////
// integrate()
// Turns on switch and computes integral of inductor voltage
////////////////////////////////////////////////////////////////////////
void integrate() {
  // Set SW1_PWM High (turn on SW1) if new_integral (just transitioned) then turn SW1 ON
  if (!new_integral) digitalWrite(SW1_PWM, HIGH);
  // Set new_integral flag to 1 (so MPPT can add when transitioned)
  new_integral = 1;
  // Read current time in ticks microseconds
  t_cur = micros();
  // Take VL Current VL Reading
  vl_cur = VL_MEAS;
  // Compute Integral sum(VL*dt)
  if (vl_cur >= vl_prev) {
    integral += (vl_prev + (vl_cur - vl_prev) / 2.0) * (t_cur - t_prev);
  } else {
    integral += (vl_cur + (vl_prev - vl_cur) / 2.0) * (t_cur - t_prev);
  }
  // Set Previous Time to Current
  t_prev = t_cur;
  // Set Previous VL to Current
  vl_prev = vl_cur;
}

////////////////////////////////////////////////////////////////////////
// mppt()
// checks to see if proper number of integrals have been averaged
// if so, then runs maximum power point tracking and modifies duty cycle
////////////////////////////////////////////////////////////////////////
void mppt() {
  // Check Battery Level
  check_battery();
  // Check Solar Level
  check_solar();
  // If don't have enough solar to charge battery
  if (v_solar * D_MAX / 100.0 < v_battery) {
    timer_on = 0;
    cur_state = DONE_CHG;
  }
  // If just transitioned to MPPT
  if (new_integral) {
    // Set SW1_PWM Low (turn SW Off)
    digitalWrite(SW1_PWM, LOW);
    // Add Integral to New Average
    integral_avg += integral;
    // Divide by 2
    integral_avg >>= 1;
    // Increment Integral Count
    num_integrals++;
    // Set New Integral to 0 (prevent re-entry)
    new_integral = 0;
    // Reset Integral Variable for next integration period
    integral = 0;
  }
  // If Have All Integrals (NUM_INT)
  if (num_integrals == NUM_INT) {
    // Compute Power
    p_cur = v_battery * integral_avg;
    // If Power Slope Positive (left of peak)
    // Did the Power Increase?
    if (p_cur - p_prev > 0) {
      // Did you Increase the Voltage (duty_cycle)?
      // Yes then increase again (max power seeking)
      if (duty_inc) {
        if (++duty_cycle >= D_MAX) duty_cycle = D_MAX;
        duty_inc = 1;
      } else {
        // Else Decrease
        if (--duty_cycle <= D_MIN) duty_cycle = D_MIN;
        duty_inc = 0;
      }
      // If Power Slope Negative (right of peak)
    } else if (p_cur - p_prev < 0) {
      // Did you Increase the Voltage (duty_cycle)?
      if (duty_inc) {
        // Yes, Then Decrease
        if (--duty_cycle <= D_MIN) duty_cycle = D_MIN;
        duty_inc = 0;
      } else {
        // Else increase again
        if (++duty_cycle >= D_MAX) duty_cycle = D_MAX;
        duty_inc = 1;
      }
    }
    // Set Previous Power to Current
    p_prev = p_cur;
    // Reset Number of Integrals
    num_integrals = 0;
#ifdef CAL
    // Printout MPPT Values
    // v_battery, v_solar, integral_avg, p_cur, duty_cycle
    sprintf(tempstr, "%f, %f, %d, %d, %d, %d", v_battery, v_solar, integral_avg, p_cur, duty_cycle, micros());
    Serial.println(tempstr);
    // Increment number of MPTTs
    n_mppt += 1;
#endif
  }
}

////////////////////////////////////////////////////////////////////////
// done_charging()
// DONE_CHG state function
// turns off timer and switch, checks to see if need to restart
////////////////////////////////////////////////////////////////////////
void done_charging() {
  // Turn Off Timer
  timer_on = 0;
  // Turn Off SW1 (disconnect solar)
  digitalWrite(SW1_PWM, LOW);
#ifdef CAL
  Serial.println("Self Test Complete");
#else
  // Check Battery and Solar
  check_battery();
  check_solar();
  // If Battery Not Charged Anymore and Solar Voltage Good, then start charging
  if ((v_battery < VCHARGE) && (v_solar * D_MAX / 100.0 >= v_battery)) cur_state = INIT_CHG;
  // Sleep for SLEEP_TIME
  // Ideally you would put the device to sleep and have some sort of RTC wake up
  // the system or better yet use an analog comparator that checks the battery
  // voltage and when it drops below charge level then wake up the system.
  delay(SLEEP_TIME * 1000);
  // Reset Device, Hard Reboot
  resetFunc();
#endif
}

////////////////////////////////////////////////////////////////////////
// charger_state_machine()
// Main charger state machine
// INIT_CHG, INTEGRATE, MPPT, and DONE_CHG states
////////////////////////////////////////////////////////////////////////
void charger_state_machine() {
#ifdef CAL
  if (n_mppt >= N_MPPT) {
    // set state to done
    cur_state = DONE_CHG;
  }
#endif
  // State Machine
  switch (cur_state) {
    ////////////////////////////////////////////////////////////////////////
    // INIT_CHG State
    // Reset variables, initialize charger
    ////////////////////////////////////////////////////////////////////////
    case INIT_CHG:
      // #IF Calibrating
#ifdef CAL
      // If calibrating
      if (calibrating) {
        calibration_state_machine();
      } else {
        // Done calibrating
        init_charger();
      }
#else
      // Initialize Charger
      init_charger();
#endif
      break;
    ////////////////////////////////////////////////////////////////////////
    // INTEGRATE State
    // Integrates Inductor voltage to (VL = L*dIL/dt -> IL = int(VL*dt)) to Get Measure of Current (L Fixed)
    ////////////////////////////////////////////////////////////////////////
    case INTEGRATE:
      // Run Integration
      integrate();
      break;
    ////////////////////////////////////////////////////////////////////////
    // MPPT State
    // Checks Where at on Power Curve and Adjusts Duty Cycle
    // Stears towards duty cycle that achieves maximum power
    ////////////////////////////////////////////////////////////////////////
    case MPPT:
      // Run MPPT
      mppt();
      break;
    ////////////////////////////////////////////////////////////////////////
    // DONE_CHG State
    // Battery Charged or Solar Dropped Out (not enough sun)
    ////////////////////////////////////////////////////////////////////////
    case DONE_CHG:
      // Complete Charging
      done_charging();
      break;
  }
}

////////////////////////////////////////////////////////////////////////
// setup_charger()
// Sets up charger GPIOs, timer, and state
////////////////////////////////////////////////////////////////////////
void setup_charger() {
  // Setup SW1 PWM as Output
  pinMode(SW1_PWM, OUTPUT);
  // Turn SW1 Off
  digitalWrite(SW1_PWM, LOW);
  // Setup ADCs as Inputs
  pinMode(VBAT_ADC, INPUT);
  pinMode(VL_ADC, INPUT);
  pinMode(VSOL_ADC, INPUT);
  // Turn off Timer
  timer_on = 0;
  // Set Current State to INIT_CHG (timer will change appropriately)
  cur_state = INIT_CHG;
  // Initialize Timer to BASE_PER (us)
  Timer1.initialize(BASE_PER * 1e6);
  // Attach Timer Intterupt Handler
  Timer1.attachInterrupt(pwm_handler);
#ifdef CAL
  // Setup Calibration
  setup_calibration();
#endif
}
