## INTRODUCTION:

This is a code with shell module interface which takes commands for enabling one of the two available hcsr sensors and stores the distance measurements until specified number of pages is filled. Galileo gen 2 development hardware is used.

## STEPS TO GIVE THE INPUT:

The shell interface has "hcsr_module"enabled. to use this module type "select hcsr_module" to enter module.
This module supports three commands.

enable n: This command is used to give the device which should be used to take measurements. There are two devices so n = 0, 1, 2 only
start p: This command is used to start fetching samples from sensor and parameter p is used to specify the number of pages to write in eeprom.
	0 <= p <= 512 only.
dump p1 p2: This command is used to retrieve pages stored in memory. pages p1 to p2 will be retrieved. p1 <= p2 <=p only.

Use the commands sequentially to check outputs

### STEPS TO COMPILE AND RUN:

The zip file contains three folders "HCSR_app", "sensor_drivers", "flash_drivers". First we need to place the drivers in appropriate folders.

**Adding sensor drivers:**
* In the sensor_drivers there are two subfolders "hcsr" and "sensor_base".
* Go to zephyr/drivers/sensor directory
* paste the hcsr folder in this directory
* replace the Kconfig and CMakesList files in zephyr/drivers/sensor folder with the Kconfig and CMakesList files of sensor_base folder

**Adding eeprom drivers:**
* Go to zephyr/drivers/flash directory
* Copy and replace the files of flash_drivers folder in the zephyr/drivers/flash directory

**Adding application folder:**
* Go to zephyr/samples directory
* copy the HCSR_app folder in this directory

#### Building the application:

* Go to the zephyr/samples/HCSR_app/build directory
* Enter the following in the terminal opened in this directory
	```
	cmake -DBOARD=galileo ../
	```
to generate MakeFile
* Now enter ```make``` in console to generate the strip file.

**NOTE** : During make if you come across any warning like "aborted for any new CONFIG paramters found" please use make oldconfig to resolve that or the drivers may not work. If while using make oldconfig it prompts for CONFIG_HCSR0_DRV_NAME and CONFIG_HCSR1_DRV_NAME. Please use the following defaults "HCSR0_DRV" and "HCSR1_DRV" respectively or the code may not work properly.

Now in build folder go to zephyr sub folder to find zephyr.strip file. Use this strip file to boot and run the application.

#### PIN CONFIGURATION AND SLAVE ADDRESSING:

By default the slave address is given as 0x52 so please connect the A0, A1, A2 pins of eeprom ic to 0(gnd),1(vcc),0 respectively.
The sda and scl pins of eeprom IC should be connected to IO18(A4) and IO19(A5) pins of galileo respectively

The gpio pin default numbering is as follows:
For device 1:
	trigger pin : IO10
	echo pin : IO3
For device 2:
	trigger pin : IO12
	echo pin : IO4

If you want to use different gpio pin numbering please go to zephyr/drivers/sensors/hcsr/hcsr_main.c file and change the macros. The description is given in the comments section of the file.
