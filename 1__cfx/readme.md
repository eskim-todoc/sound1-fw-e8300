Sample CFX Project
==================
NOTE: If you use this sample application for your own purposes, follow the licensing agreement
      specified in `Software Use Agreement  - use and accept (ONIPLAW 08142020).pdf`
      in the home directory of the installed Software Development Kit (SDK).
	
This application provides a baseline for a CFX C project within a multi-core application.
The content of the project is for illustrative purposes, showing example function prototypes,
variable initializations and corresponding headers. The code is structured as a state machine.
A structure is used to handle the context of the system and includes the state to be executed.
Additional variables can be added to this structure as needed. The main loop handles the 
changing of states.

The files in the code folder are:
-  `application_cfx.c` : main file, containing the main loop and key functions
-  `app_start.S` : assembly startup file which calls main application
-  `application_cfx_ivt.S` : application specific interrupt vector table

The file in the include folder is:
-  `application_cfx.h` : main header file

Additional project files are found in the main project folder and include this readme as well as:
- `app_LCF.bcf`: linker configuration file containing the CFX memory space mapping and other 
                    linker-required configurations

***
Copyright (c) 2023 Semiconductor Components Industries, LLC
(d/b/a onsemi).