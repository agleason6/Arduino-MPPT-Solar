# Street Solar Charger
**Charges a 12V lead-acid battery using a 100W solar panel (23Voc)**

## BE AWARE OF THE SAFETY NOTES AT THE END OF THIS README, DON'T HURT YOURSELF OR ANYBODY! 
### Batteries can leak, expode (generate Hydrogen gas), or damage what you have connected to it.

## Background
I'm homeless and living on the streets and my solar charger from Amazon (Chinese) died on me 
so I was forced to come up with a solution to get power running based off what I have in
my limited camp and can scrounge up on the streets. I wound up an inductor using a rusty palette nail
and cut the power devices off my broken charger. A wire popped off my breadboard and shorted out
the UNO I was using at first so I had to switch to a Pro Mini, both Arduinos work well. This is not the most optimal implementation 
of a solar battery charger (safety, effeciency, ...), but it works! I've been charging my batteries for 
a few weeks now and decided that I would share the implementation anyways as a learning exercise - circuits,
MPPT, embedded code development, python, ...

I implemented a calibration feature however my Arduino Pro Mini Serial RX is not working. I have attached a Debug_Notes.odt that shows what I've done to try and debug. This feature will need to be tested, however I don't have the resources. See the Secion on Calibration under Installation below for more details.

## Software Requirements
1) Arudino IDE
2) Timer1 Library

## Hardware Requirements
1) Arduino UNO or Arduino Pro Mini + 3.3V regulator (I used dead UNO)
2) Buck Converter Circuit (see Schematic)
3) 100W Solar Panel (23Voc)
4) 12V Automotive Lead Acid Battery
5) Digital Multi-meter (DMM)

## Buck Converter Circuit
**Schematic PDF in Repo**

### Power Sequence Requirement
**Ensure that you have the solar panel connected first before connecting the battery**

### Component Requirements
1) Inductor
   * You can wind up an inductor with some wire and a nail. When winding the inductor you can wind up and down (overlap) as long as you keep the direction of the turns the same (current flow). Keep turns tightly wound together and minimize gaps between turns.
2) Power Switch
   * I used a Solid State Power Relay which can only be switched at low frequencies (max around 200Hz).
     * Ensure that the switch can handle the maximum current required.
3) Opamp
   * The only real requirement for the opamp is that it be able to handle the supply voltage of the solar panel, input biased at mid supply. I happen to have an OP295 handy.
4) Instrumentation AMP
   * Same story as Oamp, the Instrumentation AMP needs to be able to handle the supply voltage of the solar panel. If you don't have an INAMP handy, you can build a differencing amplifier using opamps, just make the resistances large to not load down the input divider and bias and create offset, I used an AD620. 
3) Power Diode
   * I used the body diode of an NMOS Power FET cut off from my broken charger. The power diode needs to be able to handle the charging current.
4) 2.5V Reference
   * Must be able to handle solar open circuit voltage.

## Theory of Operation
The charger works by measuring the input solar voltage, output battery voltage, and inductor voltage 
and producing an output duty cycle signal that drives the switch of the Buck Converter.
The charger has 4 main states: INIT, INTEGRATE, MPPT, and DONE. During the INIT state it resets all 
the variables and gets ready for a new beginning. During positive duty cycle the controller is in 
the INTEGRATE state where it measures the inductor voltage and integrates it across the positive duty 
cycle period. VL = L*dIL/dt => (1/L)*VL*dt = dIL => IL(t) = (1/L)*integral(VL*dt). By integrating the 
inductor voltage you get a measure of the inductor current, not accurate, but precise for the controller 
to compare power levels (current vs previous) and make an intelligent decision. The controller has a number 
of integrals that it computes an average over before switching to the MPPT decision point, the current 
default is 10 integrals. Once the MPPT decision point is reached it takes a measurement of the battery 
voltage and multiplies it by the inductor voltage integral (linearly proportional to current) to compute 
output power point. This measurement is compared to the previously read power to see if the algorithm has 
advanced towards maximum power or not. If the slope is greater than 0 (power increased) it then asks did 
you increase the voltage (duty cycle) or not? If you increased the voltage and that was the reason for the 
power to increase then increase the voltage again, else decrease the voltage. If the power decreased 
(previous higher power), then it asks similar questions about the voltage and targets a decision to maximize 
the power. The duty cycle is either increased or decreased based on the output of the MPPT algorithm, then 
10 integral cycles are averaged again, until the battery is fully charged or it loses sun. If the solar 
voltage drops below the battery voltage (v_solar*D_MAX/100 < v_battery, D_MAX=98) then the charger will 
drop out and turn itself off and the inductor will float on the reversed biased diode (turned off); this 
is where a Boost (Buck-Boost)could help increase energy harvesting, to allow harvesting when vsolar < vbattery.

## Safety
1) Keep your battery in a well ventilated area
   * Batteries can produce H2 (Hydrogen Gas) which is extremely flammable.
2) Don't overcharge your battery
   * The chemistry behind battery charging is that when you apply a voltage to the terminals that is greater than the battery open circuit voltage (no load) it will break down the water in the battery and create H+ and O2 which breaks down the PbSO4 that forms on the terminals during discharge (creates more sulfuric acid); electrons forced into the negative terminal break down the PbSO4 on the negative terminal and create more sulfuric acid. If you apply too much voltage it can generate H2 (hydrogen gas) which is HIGHLY FLAMMABLE, if not ventilated can create a fire hazard.
3) Implement a Current Limiting Mechanism
   * To be safe, you will want to implement a current measurement and/or add a current limit, which can be done in muliple ways. 
   * Make the inductor way smaller (60x) than it needs to be based off the Buck Converter Lmin Equation
   * This prevent continous current operation and thus limit the current (current goes to 0 during negative duty cycle). 
   * Know the precise value of your inductor and divide the integral by it or add a sense resistor to the circuit or some kind of current measurement (IC) and feed that into the system; need to modify code.
   * Check it during the MPPT state, if the current gets too high then back off the duty cycle, if the current polarity changes then increase the duty cycle or change state to DONE. 
   * This algorithm neglects current and just target's maximum power, it doesn't care about what the current level is. 
4) Be aware of your manufacturer's charging recommendations for your battery, the maximum charging current limit will depend on your battery and manufacturer's recomendations for charging, heed them to be safe.  


## Installation
Ideally the user will run the firmware under calibration mode first to calibrate their implementation with a DMM, this is disabled by default until I can finish testing or someone else test and report results. This will determine the proper VBAT_COEF and VSOL_COEF that the ADCs use to convert and measure the battery and solar voltages, errors will generate false measurements. I was unable to test out this feature, and by default it is bypassed. If you would like to test this out please follow the Calibration instructions below and let me know what your results are and if you run into
any issues.

### Calibration (NEEDS TO BE TESTED!)
In the config.h file there is a "#define CAL 1". This definition opens up additonal code during compilation that first asks the user to measure the battery voltage with a DMM and input it into the Serial Monitor, then the solar voltage. The Arduino reads these values and corrects the default battery and solar ADC coeefficients and runs a self test. The self test should mimic a live MPPT tracking while printing out variable values during each MPPT: solar voltage, battery voltage, integral
average, current power, duty cycle, and time (in microseconds). The default is to run 100 MPPTs and quit. 

To run this, you will need to uncomment the #define CAL 1 line, recompile, download, and open up Serial Monitor with 115200 baud. Follow the onscreen instructions and save off the log (copy into text file).

This log which contains both the correct coefficients as well as the MPPT data. Copy these corrected coeffients back into config.h to replace the default lines and re-run calibration again to confirm that the errors are low enough and that it operates correct.

The user can import the CSV data and plot out the voltages vs time to better understand the charger dynamics.

### Live Calibrated Firmware
Once calibrated and self tested, all you need to do is comment out the #define CAL 1 line in config.h and redownload to the device. This is commented out by default, so all you need to do is follow standard Arduino code downloading procedure with Solar_Charger.ino. Once downloaded, connect up a DMM to the battery and confirm that it is charging, hence driving a higher voltage than battery open circuit voltage. Try disconnecting the battery power clip and measuring the battery voltage,
then connect again (turn on charger) and measure voltage.  
