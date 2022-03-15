/* mydrv.c */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

struct i2c_client *my_i2c;

static int fpga_i2c_xfer(struct i2c_client *i2c_client, u8 *tx_buf, u8 *rx_buf, int tx_len, int rx_len)
{ 
    int ret = 0; 
 
    struct i2c_msg msgs[] = { 
        { 
            .addr   = i2c_client->addr, 
            .flags  = 0, 
            .len    = tx_len, 
            .buf    = tx_buf,
        }, 
        { 
            .addr   = i2c_client->addr, 
            .flags  = I2C_M_RD, 
            .len    = rx_len, 
            .buf    = rx_buf, 
        }, 
    }; 
 
    if(rx_buf != NULL && tx_buf != NULL)
    {
        ret = i2c_transfer(i2c_client->adapter, msgs, 2);
    }
    else if(rx_buf != NULL)
    {
        ret = i2c_transfer(i2c_client->adapter, &msgs[1], 1);
    }
    else if(tx_buf != NULL)
    {
        ret = i2c_transfer(i2c_client->adapter, msgs, 1);
    }
 
    return ret; 
}

int my_read_reg(struct i2c_client *i2c_client, unsigned char reg_addr)
{

    struct i2c_msg msg[2];
    unsigned char write_buffer[10];
    int ret, i;
	unsigned char buffer[32];

    memset(write_buffer, 0, 10);
    memset(buffer, 0, 10);
    write_buffer[0] = (char)reg_addr;
    msg[0].addr = (i2c_client->addr);
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &write_buffer[0];
    msg[1].addr = (i2c_client->addr);
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = &buffer[0];
    
    ret = i2c_transfer(i2c_client->adapter, msg, 2);
	
	printk("0x%x\n", buffer[0]);
}

int myprobe(struct i2c_client *cli)
{
	unsigned char tx_buf[32];
	unsigned char rx_buf[32];
	
    printk("is probe!\n");
	
	my_i2c = cli;
	
	my_read_reg(my_i2c, 0x09);
	//printk("0x%x\n", rx_buf[1]);
	
    return 0;
}

int myremove(struct i2c_client *cli)
{
    printk("remove ...\n");
    return 0;
}

struct of_device_id ids[] = {
    {.compatible = "hd3ss3200"},
    {},
};

struct i2c_device_id ids2[] = {
    {"hd3ss3200"},
    {},
};

struct i2c_driver mydrv = {
    .probe_new = myprobe,
    .remove = myremove,

    .driver = {
        .owner = THIS_MODULE,
        .name = "mydrv",
      .of_match_table = ids,
    },
//    .id_table = ids2, 
};

module_i2c_driver(mydrv);
MODULE_LICENSE("GPL");