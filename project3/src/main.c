#include <zephyr.h>
#include <misc/printk.h>
#include <misc/util.h>
#include <device.h>
#include <pwm.h>
#include <board.h>
#include <pinmux_galileo.h>
#include <pinmux.h>
#include <gpio.h>
#include <kernel.h>
#include <shell/shell.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by threads for latency and cont_switch calc*/
#define PRIORITY_A 7
#define PRIORITY_B 8

/*priority of threads used for message transfer********************/
#define PRIO1 9
#define PRIO2 10


/***********************************************************************************************
MY_SHELL_MODULE : name of shell module
NUM_SAMPLES : number of measurements to print

SLEEPTIME : sleep time between context switching of threads in context_switch measurement

PWM_DRIVER : name of pwm driver for binding
IN_PIN_DRIVER : name of input pin driver for devive binding
PWM_GPIO : IO pin used for pwm generation
PWM_CHANNEL : zephyr pwm channel for configuring pwm cycles
IN_PIN : IO pin for interrupts
IN_PIN_ZEPHYR : gpio pin in zephyr ref for interrupt detection
PERIOD_PWM : period of pwm signal (fixed to 4ms = 4000us)
FREQ_GALILEO : freq of galielo (fixed = 400MHz)
PULSE COUNT : pulse count for pwm wave in latency measurement with background tasks

MAX_MSGS : max messages in message queue or length of queue
MSG_SLEEP : sleep time of message passing threads between each transfer
NUM_MESS : number of message transactions for background tasks

**************************************************************************************************/
#define MY_SHELL_MODULE "measure"

#define NUM_SAMPLES 500

#define PWM_DRIVER PINMUX_GALILEO_PWM0_NAME
#define IN_PIN_DRIVER PINMUX_GALILEO_GPIO_DW_NAME
#define OUT_PIN_DRIVER PINMUX_GALILEO_GPIO_INTEL_CW_NAME
#define PWM_GPIO 6
#define PWM_CHANNEL 5
#define IN_PIN 3
#define IN_PIN_ZEPHYR 6
#define OUT_PIN_ZEPHYR 1
#define PERIOD_PWM 4672
#define FREQ_GALILEO 400
#define PULSE_COUNT 2048

#define SLEEPTIME 5
#define MAX_MSGS 10
#define MSG_SLEEP 5
#define NUM_MESS 1000

//context_switch ovrhead due to mutex lock unlock functions and whle condition check, found using test program
#define CONT_SWITCH_OVERHEAD 2467
#define NS_IN_MSEC 1000

//for context switch
static volatile int i;

//for interrupt latency
struct gpio_callback gpback;	//gpio_callback structure instance
unsigned long long *ticks2;		//buffer to store interrupt latencies
unsigned long long *buffer;

//buffer for device driver binding of input and output pins
static struct device *in_pin_dev;	
static struct device *pinmux;
static struct device *out_pin_dev;
static struct device *pwm_dev;

unsigned long long temp;

//inline functions for readng tsc register
uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

//statically defining mutex for synchronisation
K_MUTEX_DEFINE(my_mutex);

//statically defining the stack areas for all the threads used
K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(thread1_stack_area, STACKSIZE);
static struct k_thread thread1_data;

K_THREAD_STACK_DEFINE(thread2_stack_area, STACKSIZE);
static struct k_thread thread2_data;


/*******************************************************************************************************************************/

/**********************************context switch overhead measurement********************************************************/

//common function for two synchronising threads
void helloLoop(const char *my_name)
{	
	//loop for 500 measurements
	while (i<NUM_SAMPLES) {

		k_mutex_lock(&my_mutex, K_FOREVER);
		//taking time stamp
		temp = rdtsc();	
		if(my_name[6] == 'A')
		{
			buffer[i] = temp - buffer[i];
			i++;

			if(i%50 == 0)
			printk("finished %d measurements\n",i);
		}
		
		//sleep for sometime before context switch
		k_sleep(SLEEPTIME);

		if(my_name[6]=='B')
			buffer[i] = rdtsc();

		k_mutex_unlock(&my_mutex);
	}

	//printing all the context switch overhead values
	if(my_name[6] == 'A')
	{
		for (i = 0; i< NUM_SAMPLES; i++)
			printk("latency %d : %llu nsec\n", i, ((buffer[i]*NS_IN_MSEC)/FREQ_GALILEO) - CONT_SWITCH_OVERHEAD);
			
	}

	return;
}

//thread function for thread B
void threadB_context(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	/* invoke routine to ping-pong hello messages with threadA */
	helloLoop(__func__);
}

//thread function for thread A 
void threadA_context(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	helloLoop(__func__);
}

//context switch function which is called by using cont_switch command
static int cont_switch(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("spawning threads...\n");

	i = 0;

	//spawning threads A and B
	k_thread_create(&threadA_data, threadA_stack_area, STACKSIZE,
			threadA_context, NULL, NULL, NULL,
			PRIORITY_A, 0, K_NO_WAIT);

	k_thread_create(&threadB_data, threadB_stack_area, STACKSIZE,
			threadB_context, NULL, NULL, NULL,
			PRIORITY_B, 0, K_NO_WAIT);


	return 0;

}


/*************************************************interrupt latency calculation*******************************/


//interrupt handler function for no background tasks latency measurement
void handler_nobg(struct device *driver, struct gpio_callback *cb,
		    u32_t pins)
{	
	ticks2[i] = rdtsc() - ticks2[i];

	k_mutex_lock(&my_mutex, K_FOREVER);
	i ++;
	k_mutex_unlock(&my_mutex);
	
	return;
}

//interrupt handler for latency measurement with background tasks
void handler_bg(struct device *driver, struct gpio_callback *cb,
		    u32_t pins)
{	
	ticks2[i] = rdtsc() ;
	
	k_mutex_lock(&my_mutex, K_FOREVER);
	i ++;

	if(i%50 == 0)
		printk("finished %d measurements\n",i);
	k_mutex_unlock(&my_mutex);
	
	return;
}


//thread entry function while measuring interrupt latency WITHOUT background tasks
void threadB_latency(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3); 

	int i;

	//binding the appropriate driver for output pin
	out_pin_dev = device_get_binding(OUT_PIN_DRIVER);
	//seting intial value of output pin with 0
	gpio_pin_write(out_pin_dev, OUT_PIN_ZEPHYR, 0);

	//looping for taking 500 measurements
	for(i = 0;i <NUM_SAMPLES; i++)
	{	
		ticks2[i] = rdtsc();

		//generating interrupt by raising the gpio out pin value
		gpio_pin_write(out_pin_dev, OUT_PIN_ZEPHYR, 1);
		k_sleep(SLEEPTIME);
		gpio_pin_write(out_pin_dev, OUT_PIN_ZEPHYR, 0);
		k_sleep(SLEEPTIME);

		if(i%50 == 0)
			printk("finished %d measurements\n", i);
	}

	//printing the values to console
	for(i =0; i<NUM_SAMPLES; i++)
	{
		printk("latency %d : %llu ns\n ",i, (ticks2[i] * NS_IN_MSEC)/FREQ_GALILEO);
	}

	k_free(ticks2);

	return;
}


//function for measuring interrupt latency without background tasks called by using shell command
static int latency_no_bg(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	//initialising the input and output IO pins using pinmux
	pinmux_pin_set(pinmux, IN_PIN, PINMUX_FUNC_B);
	pinmux_pin_set(pinmux, PWM_GPIO, PINMUX_FUNC_A);

	//configuring input pin for triggering interrupts
	gpio_pin_configure(in_pin_dev, IN_PIN_ZEPHYR, GPIO_DIR_IN | GPIO_INT | GPIO_INT_ACTIVE_HIGH | GPIO_INT_EDGE);

	//adding callback function for trigger pin
	gpio_init_callback(&gpback, handler_nobg, BIT(IN_PIN_ZEPHYR));
	gpio_add_callback(in_pin_dev, &gpback);
	gpio_pin_enable_callback(in_pin_dev, IN_PIN_ZEPHYR);

	//malloc buffer for storing timestamps
	ticks2 = (unsigned long long *)k_malloc(NUM_SAMPLES*sizeof(unsigned long long));
	i = 0;

	//dynamic creation of thread for performing the measurement
	k_thread_create(&threadB_data, threadB_stack_area, STACKSIZE,
		threadB_latency, NULL, NULL, NULL,
		PRIORITY_B, 0, K_NO_WAIT);

	return 0;
}

/**************************message queue background task for interrupt lat measurement with backgorund task*****************/

//structure of message block for transfer between threads
struct mess_block{
	u32_t block1;
	u32_t block2;
	u32_t block3;
};

//statically defining the message queue
K_MSGQ_DEFINE(my_queue, sizeof(struct mess_block), MAX_MSGS, 4);

//thread1 of message queue background task which sends messages
void thread1_msg(void *dummy1, void *dummy2, void *dummy3)
{	
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	struct mess_block message;
	int i,ret;
	
	message.block1 = 10;
	message.block2 = 10;
	message.block3 = 10;
	
	printk("entered thread1 function\n");

	//repeat sending messages for NUM_MESS times
	for (i =0 ; i< NUM_MESS; i++)
	{
		//placing message in queue		
		ret = k_msgq_put(&my_queue, &message, K_FOREVER);
		if(ret != 0)
		{	
			break;
			printk("message not placed\n");
		}

		k_sleep(MSG_SLEEP);
	}
	return;
}

//thread 2 function for receiving mess in message queue background task
void thread2_msg(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	struct mess_block message;
	int i,ret;

	printk("entered thread2 function\n");

	for (i =0 ; i< NUM_MESS; i++)
	{	
		//receiving messages from queue
		ret = k_msgq_get(&my_queue, &message, K_FOREVER);
		if(ret == (-ENOMSG | -EAGAIN))
		{
			printk("message not received\n");
		}

		k_sleep(MSG_SLEEP);
	}
	return;
}

//function for measuring interrupt latency WITH background tasks called by using shell command
static int latency_bg(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	//creating threads for simulating background tasks
	k_thread_create(&thread1_data, thread1_stack_area, STACKSIZE,
			thread1_msg, NULL, NULL, NULL,
			PRIO1, 0, K_NO_WAIT);
	k_thread_create(&thread2_data, thread2_stack_area, STACKSIZE,
			thread2_msg, NULL, NULL, NULL,
			PRIO2, 0, K_NO_WAIT);

	int ret;

	//configuring pwm and input pins using pinmux
	pinmux_pin_set(pinmux, IN_PIN, PINMUX_FUNC_B);
	pinmux_pin_set(pinmux, PWM_GPIO, PINMUX_FUNC_C);

	//configuring interrupt pin for triggering interrupts
	gpio_pin_configure(in_pin_dev, IN_PIN_ZEPHYR, GPIO_DIR_IN | GPIO_INT | GPIO_INT_ACTIVE_LOW | GPIO_INT_EDGE);

	//adding callbacks
	gpio_init_callback(&gpback, handler_bg, BIT(IN_PIN_ZEPHYR));
	gpio_add_callback(in_pin_dev, &gpback);
	gpio_pin_enable_callback(in_pin_dev, IN_PIN_ZEPHYR);

	//dynamic allocation of buffer memory
	ticks2 = (unsigned long long *)k_malloc(NUM_SAMPLES*sizeof(unsigned long long));

	pwm_dev = device_get_binding(PWM_DRIVER);

	if (!pwm_dev) {
		printk("Cannot find %s!\n", PWM_DRIVER);
		return 0;
	}
	i =0;

	//setting the pwm duty cycle and generating pwm wave
	ret = pwm_pin_set_cycles(pwm_dev, PWM_CHANNEL,
			     4095, PULSE_COUNT);
	if (ret ) {
		printk("pwm pin set fails %d\n", ret);
		return 0;
	}

	//looping to take 500 measurements
	while(i < NUM_SAMPLES+1)
	{	
		k_mutex_lock(&my_mutex, K_FOREVER);
		if(i == NUM_SAMPLES)
		{	
			k_mutex_unlock(&my_mutex);
			printk("i = %d\n",NUM_SAMPLES);
			pinmux_pin_set(pinmux, IN_PIN, PINMUX_FUNC_B);
			break;
		}

		k_mutex_unlock(&my_mutex);
	}

	printk("finished\n");
	for (i=0;i<NUM_SAMPLES-1;i++)
	{	
		temp = (ticks2[i+1] - ticks2[i]) ;
		printk("latency %d : %llu nsec\n",i,((temp * NS_IN_MSEC)/FREQ_GALILEO) - (PERIOD_PWM * NS_IN_MSEC));
	}

	k_free(ticks2);
	return 0;
}

/**********************************************************************************************************/

//defining shell_cmd structure for condiguring shell commands
static struct shell_cmd commands[] = {
	{ "cont_switch", cont_switch, "context switch overhead measurement" },
	{ "int_lat_nobg", latency_no_bg, "interrupt latency measurement WITHOUT background tasks" },
	{ "int_lat_bg", latency_bg, "interrupt latency measurement WITH background tasks" },
	{ NULL, NULL, NULL }
};


void main()
{
	buffer = (unsigned long long *)k_malloc(NUM_SAMPLES*sizeof(unsigned long long));
	
	//registering shell commands
	SHELL_REGISTER(MY_SHELL_MODULE, commands);

	pinmux = device_get_binding(CONFIG_PINMUX_NAME);
	in_pin_dev = device_get_binding(IN_PIN_DRIVER);
}