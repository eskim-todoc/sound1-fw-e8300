Calibration Sample Code
=======================
NOTE: If you use this sample application for your own purposes, follow the licensing agreement
      specified in `Software Use Agreement  - use and accept (ONIPLAW 08142020).pdf`
      in the home directory of the installed Software Development Kit (SDK).
	
Overview
--------
This sample project:

- Calibrates the BandGap and VREG voltage, and the LSAD gain and offset
  calibration relative to the supplied VBAT voltage for VBAT > 1.5 V. If not
  calibrating using VBAT = 1.5 V, the sample will need to be updated. These 
  elements are calibrated using a two-point calibration procedure, and set VREG
  to 900 mV.
- Calibrates the VREG, VDDM, VDDC, VDDIF, VDDA, VDDOD, and VMIC regulators, 
  the internal oscillator, and the ADCs using `calibratelib`. Inputs to most 
  calibrations includes the specified target for calibration. All components are
  calibrated to the recommended default settings. 
- Calibrates the VDDA, VDDC and VDDM power supplies relative to the PMU reference
  for use as the recommended default settings for operation in low power modes.
- Repeats voltage calibration upto 3 times for each regulator in case of a failure.
- Stores trimmings for all the calibration blocks in a local manufacturing 
  information table.
- If a non-volatile memory (NVM) with a valid file system is present in the
  system, this table is  loaded into the manufacturing area of the NVM. 
- As a final verification the local copy of the manufacturing table is cleared
  and re-loaded from NVM. If the CRC fails, an error code is returned.

Code
----
    app.c     - Includes the core applicaton code: NVM and calibration
                initialization functions, and calibration and storage function
                calls.
    start.S   - Includes application start-up code for the CFX build of the 
                application. 
    cfx_ivt.S - Includes interrupt vector table initialization, and ISR 
                definitions for the CFX build of the application.

Include
-------
    app.h     - Overall application header file

Project Structure
-----------------
This sample application includes files and build configurations for execution
on both CM3 and CFX. Simply choose the appropriate build configuration to build
for the needed core. The files required for execution on each core are outlined
below:

CM3   Code        Include    Linker Script
---   ----        -------    -------------
     app.c    app.h    sections.ld

CFX   Code        Include    Linker Script
---   ----        -------    -------------
    app.c     app.h    app_LCF.bcf
    cfx_ivt.S
    start.S
    
Hardware Requirements
---------------------
This application can be executed on a Ezairo 8300 board with accessible DIOs.
To view the output signal, connect `EVENT_DIO` (DIO4 by default) to the 
oscilloscope. 

For the VREG and LSAD calibration (required to run before calibrating the rest
of the voltage regulators), VBAT needs to be supplied externally with 1.5 V (by
default). On the SV2 Ezairo 8300 EVB the VBAT-I jumper can be taken off to supply
using an external power supply. Using accurate power supplies for this voltage
reference is recommended since the LSAD is used for calibrating the remaining 
voltage regulators.

For the clock (Internal Oscillator) calibration, a reference square wave at
128 Hz needs to be provided at `REF_CLK_DIO` (DIO11 by default).

For storing trimming values to the onboard NVM, VDDO1 should be powered using
VDDIF (1.8 V switching regulator) and the SPI lines should be connected. See
*Ezario 8300 Evaluation and Development Board Manual* for information on how to 
do this.

VDDOD is calibrated as part of this sample application, and should not be 
externally powered using from VBAT. If using the Ezairo 8300 EVB, ensure
that the OD-I header is disconnected.

The calibration routine by default uses a stabilization delay to
let the voltage being calibrated settle to the expected value just after updating
the trim. Some voltage rails such as VDDOD/VMIC/VDDIF due to the large capacitances
attached to them can take a relatively long time to stabilize. Loading these voltages by ~1mA
(attaching a 1k to 2k ohm resistor) and calling `Calibrate_Power_VoltageLoaded(true);`
can significantly reduce the stabilization time and therefore total calibration time.

Importing a Project
-------------------
To import the sample code into your IDE workspace, refer to the 
*Integrated Development Environment (IDE) User's Guide* for more information.

Verification
------------
When this application is functioning correctly, EVENT_DIO will toggle twice. For
failures, EVENT_DIO will toggle four times.

The application can also be verified in the debug environment by reading the
final value of `success`. If `success` is a zero, the application has failed.

***
Copyright (c) 2023 Semiconductor Components Industries, LLC
(d/b/a onsemi).
