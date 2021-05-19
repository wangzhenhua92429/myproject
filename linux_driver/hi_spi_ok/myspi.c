#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/spi/spi.h>

static unsigned bus_num = 2;
static unsigned csn = 0;
module_param(bus_num, uint, S_IRUGO);
MODULE_PARM_DESC(bus_num, "spi bus number");
module_param(csn, uint, S_IRUGO);
MODULE_PARM_DESC(csn, "chip select number");

struct spi_master *hi_master;
struct spi_device *hi_spi;

/**
 * @brief: 读状态寄存器
 * @param {unsigned char} reg 寄存器地址
 * @return {*} 寄存器值
 */
static unsigned char readStatusReg(unsigned char reg)
{
    unsigned char tx_buf[2];
    unsigned char rx_buf[1];
    
    tx_buf[0] = 0x05;
    tx_buf[1] = reg;

    spi_write_then_read(hi_spi, tx_buf, 2, rx_buf, 1);
    printk("val = %02x\n", rx_buf[0]);

    return rx_buf[0];
}

/**
 * @brief: 写状态寄存器
 * @param {unsigned char} reg：寄存器地址
 * @param {unsigned char} val：待写入寄存器值
 * @return {*}
 */
static void writeReg(unsigned char reg, unsigned char val)
{
    unsigned char tx_bufl[3];

    tx_bufl[0] = 0x01;
    tx_bufl[1] = reg;
    tx_bufl[2] = val;

    spi_write(hi_spi, tx_bufl, 3);
}

/**
 * @brief: 读ID
 * @param {*}
 * @return {*}
 */
static void SPIFlashReadID(void)
{
    unsigned char tx_buf[2];
    unsigned char rx_buf[4];
    
    tx_buf[0] = 0x9f;
    tx_buf[1] = 0x00;

    spi_write_then_read(hi_spi, tx_buf, 2, rx_buf, 4);
    printk("[0] = %02x...[1] = %02x...[2] = %02x\n",
        rx_buf[0], rx_buf[1], rx_buf[2]);
}

/**
 * @brief: 使能写flash
 * @param {int} enable ：0：失能 1：使能
 * @return {*}
 */
static void SPIFlashWriteEnable(int enable)
{
    unsigned char val = enable ? 0x06 : 0x04;
    spi_write(hi_spi, &val, 1);
}

/**
 * @brief: flash busy等待
 * @param {*}
 * @return {*}
 */
static void SPIFlashWaitWhenBusy(void)
{
    while (readStatusReg(0xc0) & 1)
    {
		set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ/100);
    }
}

static long temp_ioctl(struct file *pdbe_file, unsigned int dbe_cmd, unsigned long dbe_arg)
{

    return 0;
}

static const struct file_operations w25n01gw_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = temp_ioctl,
};

static struct miscdevice w25n01gw_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "w25n01gw",
    .fops = &w25n01gw_fops,
};

static int __init w25n01gw_init(void)
{
    int status = 0;
    struct device *d;
    char spi_name[12];

    hi_master = spi_busnum_to_master(bus_num);
    if(hi_master) {
        sprintf(spi_name, "%s.%u", dev_name(&hi_master->dev), csn);
        printk("spi_name = %s\n", spi_name);
        d = bus_find_device_by_name(&spi_bus_type, NULL, spi_name);
        if(d == NULL) {
            status = -ENXIO;
            goto end0;
        }

        hi_spi = to_spi_device(d);
        if(hi_spi == NULL) {
            status = -ENXIO;
            goto end1;
        }
    } else {
        status = -ENXIO;
        goto end0;
    }

    SPIFlashReadID();

    //清除写保护
    writeReg(0xa0, 0x00);

    status = misc_register(&w25n01gw_dev);
    if(status != 0)
        printk("misc_register failed!\n");

end1:
    put_device(d);
end0:
    return status;
}

static void __exit w25n01gw_exit(void)
{
    misc_deregister(&w25n01gw_dev);
}
module_init(w25n01gw_init);
module_exit(w25n01gw_exit);
MODULE_AUTHOR("Hislicon");
MODULE_LICENSE("GPL");