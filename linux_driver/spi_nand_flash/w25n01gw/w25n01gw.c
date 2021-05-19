#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <linux/spi/spi.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>

#include <linux/mtd/cfi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/device.h>

static struct spi_device* hi_spi;

static const struct of_device_id spidbg_dt_ids[] = {
    { .compatible = "w25n01gw" },
    {},
};
MODULE_DEVICE_TABLE(of, spidbg_dt_ids);

void SPIFlashReadID(int *pMID, int *pDID)
{
    unsigned char tx_buf[4];
    unsigned char rx_buf[2];
    
    tx_buf[0] = 0x9f;
    tx_buf[1] = 0;
    tx_buf[2] = 0;
    tx_buf[3] = 0;

    spi_write_then_read(hi_spi, tx_buf, 4, rx_buf, 2);

    *pMID = rx_buf[0];
    *pDID = rx_buf[1];
}

static int spi_flash_probe(struct spi_device *spi)
{
    static struct spi_master* hi_master;
    unsigned char spi_name[40];
    struct device* d;
    struct bus_type bus;
    int mid, did;

    printk("*******************************probe ok!\n");
    hi_master = spi_busnum_to_master(2);
    if(NULL == hi_master)
    {
        printk("hi_mater is NULL\n");
    }

    sprintf(spi_name, "%s.%u", dev_name(&hi_master->dev), 0);
    printk("**********************spi_name = %s\n", spi_name);
    d = bus_find_device_by_name(&spi_bus_type, NULL, spi_name);

    hi_spi = to_spi_device(d);
    //hi_spi = spi;

    SPIFlashReadID(&mid, &did);
    printk("SPI Flash ID: %02x %02x\n", mid, did);
    
    return 0;
}

static int spi_flash_remove(struct spi_device *spi)
{
    //mtd_device_unregister(&spi_flash_dev);
    return 0;
}

static struct spi_driver spi_flash_drv = {
    .driver = {
        .name	= "w25n01gw",
        .of_match_table = of_match_ptr(spidbg_dt_ids),
    },
    .probe		= spi_flash_probe,
    .remove		= spi_flash_remove,
};

static int __init w25n01gw_init(void)
{
    return spi_register_driver(&spi_flash_drv);
}
static void __exit w25n01gw_exit(void)
{
    spi_unregister_driver(&spi_flash_drv);
}

module_init(w25n01gw_init);
module_exit(w25n01gw_exit);

MODULE_LICENSE("GPL");
