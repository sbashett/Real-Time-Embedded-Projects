## INTRODUCTION:

This is a code which measures and prints the context switch overhead and interrupt latencies with and without background tasks in zephyr RTOS environment. Target platform is intel galileo gen2.

## STEPS TO GIVE THE INPUT:

The program first needs to be built and then, the generated .strip file should be placed in SD card for booting. Once the zephyr RTOS is booted, the shell console waits for commands.

* First we need to enter the module by using the command "select measure". measure is name of module
* There are three commands in this module:
	* cont_switch : which measures and prints the context switch overheads.
	* int_lat_nobg : measures and prints the interrupt latencies without background tasks.
	* int_lat_bg : measures and prints interrupt latencies with background tasks

Type any of the command for printing respective measurements

**NOTE** : PLEASE REBOOT THE BOARD BEFORE TESTING EACH COMMAND

### PIN CONNECTIONS:

Here IO6 pin is used as output pin and IO3 pin as input by default. Short both the pins before running program. You can change the pins by changing the appropriate macros in the main.c file. The macros are clearly described in the main.c file comments section. Change the macros as per necessity.

SHORT IO6 AND IO3 PINS OR CHANGE MACROS AS NEEDED

### STEPS TO COMPILE AND BUILD:

Follow the steps to compile and build the .strip file

Place the measure_7 folder in zephyr/samples directory

There are two folders in measure_7, build and src. The src folder contains the main.c file which is the main program.

* Open the terminal in build folder and enter the following commands

	```
	cmake -DBOARD=galileo ../
	```
	to generate the makefile using cmake environment

* Enter ```make``` command to build the .strip file

	After building the .strip file go to zephyr folder and place the generated .strip file in the SD card for booting

* DO NOT FORGET TO SHORT IO3 AND IO6 PINS
