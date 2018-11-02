#include <flash.h>
#include <device.h>
#include <zephyr.h>
#include <stdio.h>
#include <misc/printk.h>
#include <syscall.h>
#include <sensor.h>
#include <shell/shell.h>
#include <stdlib.h>

#define STACKSIZE 1024 	//defining stack size for threads
#define SENSOR_PRIORITY 6	//defining priority of sensor thread
#define EEPROM_PRIORITY 6	//defining priority of eeprom thread
#define BUFFER_LENGTH 64	//defining buffer length

#define MY_SHELL_MODULE "hcsr_app"

static int device_num,pages;	//global variables for storing number of pages and device num to use
static uint8_t *buff1, *buff2;	//two buffers to parllely fetch senor data and write into eeprom
static uint8_t *temp1, *temp2;	//two variables to temporarily store the buffers pointers
atomic_t buff1_ind = ATOMIC_INIT(0);	//atomic variables for synchronisation between threads
atomic_t buff2_ind = ATOMIC_INIT(0);
extern const k_tid_t hcsr_threadid;	//variables for storing thread ids
extern const k_tid_t eeprom_writeid;

//shell callback function for enable command
static int shell_cmd_enable(int argc, char *argv[])
{
	if((argc!=2))
	{
		printk("enter correct number of inputs for enable\n");
		printk("argc: %d", argc);
		return 0;
	}

	printk("entered value : %s", argv[1]);

	//storing the device input from command line and storing in global variable
	device_num = atoi(argv[1]);

	if((device_num == 0) || (device_num > 2))
	{
		printk("no devices initiated or invalid input\n");
		return 0;
	}

	//dynamic memory allocation of buffers
	buff1 = k_malloc(BUFFER_LENGTH);
	buff2 = k_malloc(BUFFER_LENGTH);

	printk("erasing eeprom....\n");
    struct device *dev = device_get_binding(CONFIG_EEPROM_DRV_NAME);
	flash_erase(dev, 0x00, NULL);

	return 0;
}

//shell call back function for the start command 
static int shell_cmd_start(int argc, char *argv[])
{
    
    if((argc!=2))
    {
    	printk("enter correct number of parameters \n");
    	return 0;
    }

    //reading the input and determining no)of pages to write
    pages = atoi(argv[1]);

    //checking validity of inputs
    if((pages <= 0) || (pages > 512))
    {
    	printk("enter number of pages in range of 1 to 512\n");
    	return 0;
    }

    temp1 = buff1;
    temp2 = buff1;

    //checking validity of device number
    if((device_num <= 0) || (device_num >= 3))
    {
      	printk("please enable the correct device\n");
      	return -1;
    }

    //starting the sensor and eeprom threads
    k_thread_start(hcsr_threadid);
	printk("reached here\n");
    k_thread_start(eeprom_writeid);

	return 0;
}

//shell call back function for dump command which prints out all the values
static int shell_cmd_dump(int argc, char *argv[])
{
    uint8_t cmp_buf[64];
    int page1, page2, i, j;
    u16_t offset;

    if((argc!=3))
    {
    	printk("enter correct number of parameters \n");
    	return 0;
    }

    //reading the input page numbers to print from
    page1 = atoi(argv[1]);
    page2 = atoi(argv[2]);

    //checking the validity of page numbers
    if( (page1 < 0) || (page1 > pages) || (page2 < 0) || (page2 > pages) || (page1 > page2))
    {
    	printk("enter p1, p2 in range of 0,%d and with p1<=p2",pages);
    	return 0;
    }

    //initializing the offset based on initial page number 
    offset = 0x00 + (page1 * 0x40);
    struct device *dev = device_get_binding(CONFIG_EEPROM_DRV_NAME);
    u32_t distance,stamp;

    //loop over given page numbers range
    for(i = page1; i<= page2; i++)
    {
		if(flash_read(dev,offset,cmp_buf,BUFFER_LENGTH)!= 0 )
			{
			printk("error\n");
			}

		printk("page %d", i);

		//loop over the 64 byte buffer
		for(j = 0; j<64; j+=8)
		{	

			//combing the buffer bytes to form the distance and time measurement
			distance = stamp = 0;
			distance = distance | cmp_buf[j];
			distance = distance | (cmp_buf[j+1] << 8);
			distance = distance | (cmp_buf[j+2] << 16);
			distance = distance | (cmp_buf[j+3] << 24);

			stamp = stamp | cmp_buf[j+4];
			stamp = stamp | (cmp_buf[j+5] << 8);
			stamp = stamp | (cmp_buf[j+6] << 16);
			stamp = stamp | (cmp_buf[j+7] << 24);

			printk("distance %d, time from start: %dus\n", distance, stamp);
		}

		//incrementing offset by 1 page
		offset += 0x40;
	}

	return 0;
}

//thread funciton for fetchig and storing sensor data
void hcsr_thread(void)
{
	struct sensor_value temp;
	struct device *dev1;

	//selecting the device based on input
	if(device_num == 1)
		dev1 = device_get_binding(CONFIG_HCSR0_DRV_NAME);
	else if(device_num == 2)
		dev1 = device_get_binding(CONFIG_HCSR1_DRV_NAME);
	else
		return;

	printk("entered sensor thread for fetching data\n");

	int i,j,cnt;

	//loop over number of pages
	for(i =0; i < pages; i++)
	{	
		cnt = 0;

		//checking if empty buffers are avialable
		while((atomic_get(&buff1_ind) == 1) && (atomic_get(&buff2_ind) == 1));
		
		if(atomic_get(&buff1_ind) == 0)
			temp1 = buff1;
		else
			temp1 = buff2;

		//looping over to store 1 page of information
		for (j =0 ; j<8; j++)
		{
			sensor_sample_fetch(dev1);			
			sensor_channel_get(dev1,SENSOR_CHAN_ALL, &temp);

			printk("distace : %d\n", temp.val1);
			//storing the 32 bit distance and stamp into 8 array elemnts
			temp1[cnt] = (temp.val1) & 0xFF;
			temp1[cnt+1] = (temp.val1 >> 8) & 0xFF;
			temp1[cnt+2] = (temp.val1 >> 16) & 0xFF;
			temp1[cnt+3] = (temp.val1 >> 24) & 0xFF;
			cnt += 4;
			temp1[cnt] = (temp.val2) & 0xFF;
			temp1[cnt+1] = (temp.val2 >> 8) & 0xFF;
			temp1[cnt+2] = (temp.val2 >> 16) & 0xFF;
			temp1[cnt+3] = (temp.val2 >> 24) & 0xFF;
			cnt += 4;
		}

		if(temp1 == buff1)
			atomic_set(&buff1_ind,1);
		else
			atomic_set(&buff2_ind,1);			
	}
}

//thread function for writing data into eeprom
void eeprom_write(void)
{
	struct device *dev = device_get_binding(CONFIG_EEPROM_DRV_NAME);
	u16_t offset;

	printk("entered eeprom_write thread\n");
	if(!dev){
		printk("eeprom device not found\n");
		return;
	}
	int i,j;
	offset = 0x00;

	//loop over number of pages
	for (i =0; i<pages; i++)
	{	
		
		//checking for any full buffers
		while(1)
		{
			if(atomic_get(&buff1_ind) == 1)
			{
				temp2 = buff1;
				break;
			}
			else if(atomic_get(&buff2_ind) == 1)
			{
				temp2 = buff2;
				break;
			}
			else
				continue;
		}

		printk("writing page %d", i+1);	

		//writing the buffer data into eeprom using i2c and flash device interface
		if(flash_write(dev,offset,temp2,BUFFER_LENGTH)!=0)
			{
				printk("error\n");
			}

		if(temp2 == buff1)
			atomic_set(&buff1_ind,0);
		else
			atomic_set(&buff2_ind,0);

		offset += 0x40;
	}

	return;
}

////defining chell command strucure
static struct shell_cmd commands[] = {
    { "enable", shell_cmd_enable, "give the input in the format enable n; n= 1,2 to give dev_num" },
    { "start", shell_cmd_start, "give the input with format start p; 0<p<512 to give num_pages" },
	{ "dump", shell_cmd_dump, "give the inputs in format dump p1 p2; p1 < p2 < p to give pages to print" },
    { NULL, NULL, NULL },
};

void main()
{

	SHELL_REGISTER(MY_SHELL_MODULE, commands);
}

//creating static threads for eeprom and sensor threads
K_THREAD_DEFINE(hcsr_threadid, STACKSIZE, hcsr_thread, NULL, NULL, NULL, 
				SENSOR_PRIORITY, 0, K_FOREVER);

K_THREAD_DEFINE(eeprom_writeid, STACKSIZE, eeprom_write, NULL, NULL, NULL, 
				EEPROM_PRIORITY, 0, K_FOREVER);
