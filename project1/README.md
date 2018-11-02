## INTRODUCTION:

This is a code which takes input from a text file written in a specific format(can be seen from the given example). It takes total no:of periodic and aperiodic tasks as input and assigns the thread functions respectively with the given real time prorities. The thread function is a generalised function which takes prameters to increase/decrease the no:of computation blocks and even the locking and unlocking of critical sections based on input. Priority inversion and inheritance have been demonstrated using a custom example and the detail description is presented in the report. Target platform is intel galileo gen2.

### STEPS TO GIVE THE INPUT:

The input to the program can be given using a text file. By default it should be named as "input.txt". If we want to change the name of input file we can do it by changing the name in main code. There is an input.txt file by default which contains our custom inputs on which we tested the priority inheritance and showed the results in report.

### STEPS TO COMPILE AND RUN:

A "program.c" file contains the code and we can compile the program using the provided MakeFile. Type following commands in terminal opened in the directory containing code and MakeFile

* For compiling for the host platform use the command ```make```

* For cross compilation use ```make 'CC=i586-poky-linux-gcc'```
where i586-poky-linux-gcc is a 32-bit cross compilation toolchain for yocto project SDK or linux kernel. For details refer [link](https://www.yoctoproject.org/docs/2.1.1/sdk-manual/sdk-manual.html#sdk-intro).

An object file will be created with name "program.o". We need to run the program with root privileges in host platform using command ```sudo ./program.o``` from object file location.

we can run it normally in the target platform after copying the cross compiled object file and input.txt to target using scp, with the command ```./program.o```

### ENABLING PRIORITY INHERITANCE:

To enable priority inheritance for mutex locks we need to change the PI_ENABLE macro in the program.c to '1' before compiling. By default the priority inheritance is disabled.

### ADDITIONAL COMMENTS:

* Please copy the input.txt file to target before testing the code on target.
* Please change the macro value to enable priority inheritance.
