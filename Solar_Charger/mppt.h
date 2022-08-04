////////////////////////////////////////////////////////////////////////
// mppt.h
// MPPT Header File
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
// Config Header
#include "config.h"

// If Calibration Firmware
#ifdef CAL
// Calibration Header
#include "calibration.h"
#endif

////////////////////////////////////////////////////////////////////////
// Macros
////////////////////////////////////////////////////////////////////////
// Battery Voltage ADC Macro
#define VBAT_MEAS (analogRead(VBAT_ADC)*VBAT_COEF)
// Inductor Voltage ADC Macro
#define VL_MEAS ((analogRead(VL_ADC)*ADC_COEF + VL_OFF)*VL_COEF)
// Solar Voltage ADC Macro
#define VSOL_MEAS (analogRead(VSOL_ADC)*VSOL_COEF)

////////////////////////////////////////////////////////////////////////
// Type Definitions
////////////////////////////////////////////////////////////////////////
// State Variable Type Definition
typedef enum _states {INIT_CHG, INTEGRATE, MPPT, DONE_CHG} STATES;

////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////
// State Variable Definition
extern volatile STATES cur_state;
// Duty Cycle Variable
extern volatile unsigned char duty_cycle;
// Solar and Battery Voltage Variables
extern volatile double v_solar, v_battery;
// Inductor Current and Previous Voltage Variables
extern volatile double vl_cur, vl_prev;
// MPPT Power Tracking Variables (for slopes)
extern volatile double p_cur, p_prev;
// Integral Variable
extern volatile long int integral;
// Time Tracking Variables (for dt integration)
extern volatile unsigned long int t_cur, t_prev;
// Number of Integrations
extern volatile unsigned char num_integrals;
// Average Integral Value (over NUM_INT integrals)
extern volatile long int integral_avg;
// PWM Count Variable
extern volatile unsigned char pwm_count;
// New Integral Flag, Timer On Flag, and Duty Cycle Increase Flag
extern volatile bool new_integral, timer_on, duty_inc;

////////////////////////////////////////////////////////////////////////
// Function Prototypes
////////////////////////////////////////////////////////////////////////
extern void check_battery();
extern void check_solar();
extern void pwm_handler();
extern void charger_state_machine();
extern void init_charger();
extern void integrate();
extern void mppt();
extern void done_charging();
extern void setup_charger();
