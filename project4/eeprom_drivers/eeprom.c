#include <zephyr.h>
#include <flash.h>
#include <i2c.h>
#include <misc/printk.h>
#include <device.h>
#include <init.h>

#define EEPROM_SLAVE_ADDR 0x52	//eeprom salve address

#define I2C_DRV_NAME CONFIG_I2C_0_NAME 	//config name 

//function for reading data from eeprom using i2c
static int read_eeprom(struct device *dev, off_t offset, void *data,
			     size_t len)
{
	int ret;
	struct device *i2c_dev = device_get_binding(I2C_DRV_NAME);
	u8_t addr[2];
	struct i2c_msg msgs[2];

	//forming i2c messages for reading the data fro specified address into a buffer
	addr[0] = (offset >> 8) & 0xFF;
	addr[1] = offset & 0xFF;

	//frst i2c message is address
	msgs[0].buf = addr;
	msgs[0].len = 2;
	msgs[0].flags = I2C_MSG_WRITE;

	//second i2c message gives the buffer pointer to store the read values
	msgs[1].buf = data;
	msgs[1].len = len;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	//transferring i2c messages using i2c api
	ret =  i2c_transfer(i2c_dev, &msgs[0], 2, EEPROM_SLAVE_ADDR);

	printk("ret val in read %d\n",ret);

	return 0;
}

//function for writing data into eeprom
static int write_eeprom(struct device *dev, off_t offset,
			      const void *data, size_t len)
{
	int ret;
	struct device *i2c_dev = device_get_binding(I2C_DRV_NAME);
	u8_t addr[2];
	struct i2c_msg msgs[2];

	//forming i2c messages to write data to specifed address from data buffers
	addr[0] = (offset >> 8) & 0xFF;
	addr[1] = offset & 0xFF;

	//first i2c message is memory address to write to
	msgs[0].buf = addr;
	msgs[0].len = 2;
	msgs[0].flags = I2C_MSG_WRITE;

	//second i2c message is actual data to write to
	msgs[1].buf = data;
	msgs[1].len = len;
	msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	//using i2c transfer to initiate data transfer
	ret =  i2c_transfer(i2c_dev, msgs, 2, EEPROM_SLAVE_ADDR);
	//printk("ret val in write %d\n",ret);
	return 0;
}

static int erase_eeprom(struct device *dev, off_t offset, size_t size)
{
	int ret;
	struct device *i2c_dev = device_get_binding(I2C_DRV_NAME);
	u8_t addr[2];
	struct i2c_msg msgs[2];
	u8_t data[64] = {0};
	int len = 64;

	return 0;

	//forming i2c messages to write data to specifed address from data buffers
	addr[0] = (offset >> 8) & 0xFF;
	addr[1] = offset & 0xFF;

	//first i2c message is memory address to write to
	msgs[0].buf = addr;
	msgs[0].len = 2;
	msgs[0].flags = I2C_MSG_WRITE;

	//second i2c message is actual data to write to
	msgs[1].buf = data;
	msgs[1].len = len;
	msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	//using i2c transfer to initiate data transfer
	ret =  i2c_transfer(i2c_dev, msgs, 2, EEPROM_SLAVE_ADDR);
	//printk("ret val in write %d\n",ret);
	return 0;
}

static const struct flash_driver_api eeprom_api = {
	.read = read_eeprom,
	.write = write_eeprom,
	.erase = erase_eeprom,
	// .write_protection = spi_flash_wb_write_protection_set,
	// .write_block_size = 1,
};

//init function for eeprom drivers called at boot time
static int eeprom_init(struct device *dev)
{
	dev->driver_api = &eeprom_api;
	return 0;
}

//initializing eeprom deivce at boot time
DEVICE_INIT(eeprom_wsxxx, CONFIG_EEPROM_DRV_NAME, eeprom_init,
	    NULL, NULL, POST_KERNEL,
	    CONFIG_EEPROM_INIT_PRIORITY);

