
#include <device.h>
#include <sensor.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <misc/util.h>
#include <pwm.h>
#include <board.h>
#include <pinmux_galileo.h>
#include <pinmux.h>
#include <gpio.h>
#include <kernel.h>
#include <stdbool.h>

#define PINMUX_DRIVER CONFIG_PINMUX_NAME	//config name for binding pinmux driver

#define ECHO_DEVICE_HCSR0 PINMUX_GALILEO_GPIO_DW_NAME 		// The name of the that device for echo
#define ECHO_DEVICE_HCSR1 PINMUX_GALILEO_GPIO_INTEL_RW_NAME 		// The name of the that device for echo
#define IO_ECHO_PIN_HCSR0 3						// The IO pin number for hcsr0 echo 
#define GPIO_ECHO_PIN_HCSR0 6						// The pin number on that particular port of the chip
#define IO_ECHO_PIN_HCSR1 4						// The IO pin number for hcsr1 echo 
#define GPIO_ECHO_PIN_HCSR1 4						// The pin number on that particular port of the chip


#define TRIGGER_DEVICE_HCSR0 PINMUX_GALILEO_GPIO_DW_NAME		// The name of the that device for trigger
#define TRIGGER_DEVICE_HCSR1 PINMUX_GALILEO_GPIO_DW_NAME		// The name of the that device for trigger
#define IO_TRIGGER_PIN_HCSR0 10					// The IO pin number for hcsr0 trigger 
#define GPIO_TRIGGER_PIN_HCSR0 2					// The pin number on that particular port of the chip  
#define IO_TRIGGER_PIN_HCSR1 12					// The IO pin number for hcsr1 trigger 
#define GPIO_TRIGGER_PIN_HCSR1 7					// The pin number on that particular port of the chip  


#define OUT_FN PINMUX_FUNC_A 	//pimnuc fucntion for initialzing gpio as output
#define IN_FN PINMUX_FUNC_B  	//pimnuc fucntion for initialzing gpio as input

#define HIGH 1
#define LOW 0
#define CPU_FREQUENCY 400
#define US_IN_S 1000000
#define SPEED_SOUND_CM 33000
#define DIV_FACTOR 23200

K_MUTEX_DEFINE(lock);	//lock for synchronisation
unsigned long delay1,delay2,delay3,delay4, ini_stamp;	//global variables for storing time stamps
static bool frst_flag;	
struct device *trigger_hcsr0=NULL;	//global device pointers for trigger pins
struct device *trigger_hcsr1=NULL;

//interrupt handler funciton for echo signal
static void handler(struct device *dev, struct gpio_callback *cb, u32_t pins)
{	
	//printk("entered\n");
	delay2 = _tsc_read();
	delay4 = _tsc_read();
	k_mutex_unlock(&lock);
}

// function for fetching the data from sensors
static int hcsr_sample_fetch(struct device *dev, enum sensor_channel chan)
{	
	if(frst_flag == 0)
	{
		ini_stamp = _tsc_read();
		frst_flag = 1;
	}

	//checking the device name for determind the device to be used
	if(dev->config->name[4] == '0')
	{//block executed for hcsr0 device
		// TRIGGER 
		gpio_pin_write(trigger_hcsr0,GPIO_TRIGGER_PIN_HCSR0,HIGH);
		//wait for 10us  
		k_busy_wait(10); 
		gpio_pin_write(trigger_hcsr0,GPIO_TRIGGER_PIN_HCSR0,LOW);
		delay1=_tsc_read();
		//printk("TIMESTAMP1 %lu\n",delay1);
	}

	else
	{//block executed for hcsr1 device
		printk("%s entered fetch\n", "hcsr1");
		// TRIGGER 
		gpio_pin_write(trigger_hcsr1,GPIO_TRIGGER_PIN_HCSR1,HIGH);
		//wait for 10us  
		k_busy_wait(10); 
		gpio_pin_write(trigger_hcsr1,GPIO_TRIGGER_PIN_HCSR1,LOW);
		delay3=_tsc_read();
		//printk("TIMESTAMP1 %lu\n",delay3);
	}	

	k_mutex_lock(&lock, K_NO_WAIT);

	return 0;
}

//function for sending the stored sensor value to application program
static int hcsr_channel_get(struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{

	//unsigned long diff,val2;
	int device;
	
	//checking the entered device number
	if(dev->config->name[4] == '0')
	{	//block executed for first device
		k_mutex_lock(&lock, K_NO_WAIT); 
		//printk("entered 1\n");
		device = 1;
	}
	else
	{//block executed for second device
		k_mutex_lock(&lock, K_NO_WAIT); 
		printk("entered 2\n");
		device = 2;
	}

	k_mutex_unlock(&lock);

	//calculating the distance
	//diff = ((diff / CPU_FREQUENCY) * SPEED_SOUND_CM) / US_IN_S ;

	// printk("timestamp before : %lu ", delay1);
	// printk("time stamp after : %lu ", delay2);

	if (device == 1)
	{	
		printk("before_stamp : %lu, after_stamp: %lu\n", delay1, delay2);
		//k_busy_wait(10);
		val->val1 = ((delay2-delay1)/DIV_FACTOR) - 8;
		val->val2 = (delay2 - ini_stamp)/CPU_FREQUENCY;
	}
	else
	{	
		printk("before_stamp : %lu, after_stamp: %lu\n", delay3, delay4);
		val->val1 = ((delay4-delay3)/DIV_FACTOR)-8;
		val->val2 = (delay4 - ini_stamp)/CPU_FREQUENCY;
	}
	return 0;
}

//mapping the funcions to sesor api
static const struct sensor_driver_api hcsr_driver_api = {
	.sample_fetch = hcsr_sample_fetch,
	.channel_get = hcsr_channel_get,
};

//funciton for configuring the eco pins of devices
bool hcsr_echo_conf(struct device *dev)
{
	int ret =0;
	struct device *pinmux=NULL;
	struct device *echo=NULL;

	//declaring call back structure
	struct gpio_callback *cb=k_malloc(sizeof(struct gpio_callback));

	if(!cb)
	{
		printk("Can't allocate mem on the heap");
		ret=1;
		goto ERR;
	}

	pinmux = device_get_binding(PINMUX_DRIVER);
	if(!pinmux)

		{
			printk("Can't get the driver binding for Pinmux ");
			ret=1;
			goto ERR;
		}

	//checking the device for enabling interrupts
	if(dev->config->name[4] == '0')
	{
		echo = device_get_binding(ECHO_DEVICE_HCSR0);

		if(!echo)

			{
				printk("Can't get the driver binding for %s",ECHO_DEVICE_HCSR0);
				ret=1;
				goto ERR;
			}

		//initialising gpio using pinmux funcitons
		ret = pinmux_pin_set(pinmux,IO_ECHO_PIN_HCSR0,IN_FN);

		//configuring interrupts for the gpio
		ret = gpio_pin_configure(echo,GPIO_ECHO_PIN_HCSR0,GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW );

		if(ret)
			{
				printk("Gpio Interrupt configure failed");
				ret=1;
				goto ERR;
			}

		//init callbacks
		gpio_init_callback(cb, handler, BIT(GPIO_ECHO_PIN_HCSR0));
		        
		if (gpio_add_callback(echo, cb)) 
		{
		    printk("Set callback fail\n");
		    ret=1;
			goto ERR;
	    }

		gpio_pin_enable_callback(echo,GPIO_ECHO_PIN_HCSR0);
	}

	//block if device 2 is enabled
	else
	{
		echo = device_get_binding(ECHO_DEVICE_HCSR1);
		if(!echo)

			{
				printk("Can't get the driver binding for %s",ECHO_DEVICE_HCSR0);
				ret=1;
				goto ERR;
			}
		
		ret = pinmux_pin_set(pinmux,IO_ECHO_PIN_HCSR1,IN_FN);

		//configuring gpio as input and enabling interrupts
		ret = gpio_pin_configure(echo,GPIO_ECHO_PIN_HCSR1,GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW );
		if(ret)
		{
			printk("Gpio Interrupt configure failed");
			ret=1;
			goto ERR;
		}

		//initializing callback
		gpio_init_callback(cb, handler, BIT(GPIO_ECHO_PIN_HCSR1));
		if (gpio_add_callback(echo, cb)) 
		{
		    printk("Set callback fail\n");
		    ret=1;
			goto ERR;
	    }

		gpio_pin_enable_callback(echo,GPIO_ECHO_PIN_HCSR1);
	}

	ERR : 

	return ret;
}

//function for configuring trigger pins of devices
bool hcsr_trigger_conf(struct device *dev)
{

	int ret=0;
	struct device *pinmux=NULL;

	pinmux = device_get_binding(PINMUX_DRIVER);

	if(!pinmux)
		{
			printk("Can't get the driver binding for Pinmux ");
			ret=1;
		}

	//device specific gpio initialization
	if(dev->config->name[4] == '0')	
		ret = pinmux_pin_set(pinmux,IO_TRIGGER_PIN_HCSR0,OUT_FN);
	else
		ret = pinmux_pin_set(pinmux,IO_TRIGGER_PIN_HCSR1,OUT_FN);

	return ret;
}

//init function for sensor driver 
static int hcsr_init(struct device *dev)
{
	dev->driver_api = &hcsr_driver_api;

	//calling function for configuring trigger and echo pins
	if(hcsr_trigger_conf(dev))
	{
		printk("Trigger config failed");
		return -1;
	}

	if(hcsr_echo_conf(dev))
	{
		printk("Echo config failed");
		return -1;
	}

	frst_flag = 0;

	//getting the device binding for trigger gpio pins
	trigger_hcsr0 = device_get_binding(TRIGGER_DEVICE_HCSR0);	
	trigger_hcsr1 = device_get_binding(TRIGGER_DEVICE_HCSR1);	

	
	if(!trigger_hcsr0 || !trigger_hcsr1)
	{
		printk("Can't get the driver binding for one of the devices\n");
		return -1;
	}

	return 0;
}


//initializing sensor devices at boot time
DEVICE_INIT(hcsr0, CONFIG_HCSR0_DRV_NAME, hcsr_init,
		    NULL, NULL,
		    POST_KERNEL,
		    CONFIG_SENSOR_INIT_PRIORITY);

DEVICE_INIT(hcsr1, CONFIG_HCSR1_DRV_NAME, hcsr_init,
		    NULL, NULL,
		    POST_KERNEL,
		    CONFIG_SENSOR_INIT_PRIORITY);