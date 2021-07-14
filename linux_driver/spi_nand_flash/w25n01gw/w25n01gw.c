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

static unsigned bus_num = 2;
static unsigned csn = 0;
module_param(bus_num, uint, S_IRUGO);
MODULE_PARM_DESC(bus_num, "spi bus number");
module_param(csn, uint, S_IRUGO);
MODULE_PARM_DESC(csn, "chip select number");

#define SPI_FLASH_COLUMN_SIZE (512)
#define SPI_FLASH_PAGE_SIZE   (4 * SPI_FLASH_COLUMN_SIZE) // 2048B/page
#define SPI_FLASH_BLOCK_SIZE  (64 * SPI_FLASH_PAGE_SIZE)  //64page

struct spi_master *hi_master;
struct spi_device *hi_spi;
static struct mtd_info spi_flash_dev;

/* 读出设备ID */
void SPIFlashReadID(void)
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
 * 读状态寄存器
*/
unsigned char readReg(unsigned char reg)
{
    unsigned char tx_buf[2];
    unsigned char rx_buf[1];
    
    tx_buf[0] = 0x05;
    tx_buf[1] = reg;

    spi_write_then_read(hi_spi, tx_buf, 2, rx_buf, 1);
    //printk("val = %02x\n", rx_buf[0]);

    return rx_buf[0];
}

/**
 * 写状态寄存器
*/
void writeReg(unsigned char reg, unsigned char val)
{
    unsigned char tx_bufl[3];

    tx_bufl[0] = 0x01;
    tx_bufl[1] = reg;
    tx_bufl[2] = val;

    spi_write(hi_spi, tx_bufl, 3);
}

unsigned char SPIChangeReg3Buf(void)
{
    unsigned char tx_buf[2];
    unsigned char rx_buf[1];
    
    tx_buf[0] = 0x05;
    tx_buf[1] = 0xc0;

    spi_write_then_read(hi_spi, tx_buf, 2, rx_buf, 1);
    //printk("status reg3 = 0x%02x\n", rx_buf[0]);

    return rx_buf[0];
}

static void SPIFlashWaitWhenBusy(void)
{
    while (SPIChangeReg3Buf() & 1)
    {
        //printk("flash is busy! reg3 = 0x%02x\n", SPIChangeReg3Buf());

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ/100);  /* 休眠10MS后再次判断 */
    }
}

static void SPIFlashWriteEnable(int enable)
{
    unsigned char val = enable ? 0x06 : 0x04;
    spi_write(hi_spi, &val, 1);
}

void SPIFlashEraseSector(unsigned int addr)
{
    unsigned char tx_buf[4];
    tx_buf[0] = 0xd8;
    tx_buf[1] = 0x00;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    SPIFlashWriteEnable(1);

    spi_write(hi_spi, tx_buf, 4);

    SPIFlashWaitWhenBusy();
}

static int my_flash_erase(struct mtd_info *mtd, struct erase_info *instr)
{
    unsigned int addr_start = instr->addr / SPI_FLASH_BLOCK_SIZE;
    unsigned int addr_end;
    unsigned int addr = instr->addr;
    int i = 0;

    printk("***************************erase!\n");

    if ((addr % SPI_FLASH_PAGE_SIZE) || (instr->len % SPI_FLASH_PAGE_SIZE))
    {
        printk("[%s]addr/len is not aligned\n", __func__);
        return -EINVAL;
    }
    
    addr_end = instr->len / SPI_FLASH_PAGE_SIZE;
    addr = addr_start;
    for (i = 0; i < addr_end; i++)
    {
        printk("erase %d page!\n", i);
        SPIFlashEraseSector(addr);
        addr += SPI_FLASH_PAGE_SIZE * i;
    }
    
    instr->state = MTD_ERASE_DONE;
    mtd_erase_callback(instr);

    return (((readReg(0xc0) >> 2) & 0x1) == 0 ? 0 : -1);
}

void SPIFlashPageRead(unsigned int addr)
{
    unsigned char tx_buf[4];
    tx_buf[0] = 0x13;
    tx_buf[1] = 0x00;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    spi_write(hi_spi, tx_buf, 4);

    SPIFlashWaitWhenBusy();
}

void SPIFlashRead(unsigned int addr, unsigned char *buf, int len)
{
    unsigned char tx_buf[4];   
    struct spi_transfer	t[] = {
            {
                .tx_buf		= tx_buf,
                .len		= 4,
            },
            {
                .rx_buf		= buf,
                .len		= len,
            },
        };
    struct spi_message	m;

    tx_buf[0] = 0x03;
    tx_buf[1] = addr >> 8;
    tx_buf[2] = addr & 0xff;
    tx_buf[3] = 0x00;

    spi_message_init(&m);
    spi_message_add_tail(&t[0], &m);
    spi_message_add_tail(&t[1], &m);
    spi_sync(hi_spi, &m);

    SPIFlashWaitWhenBusy();
}

static int my_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
        size_t *retlen, u_char *buf)
{
    unsigned int addr = from;
    unsigned int addr_page = 0;
    unsigned int rlen = 0;
    int i = 0;

    if ((addr % SPI_FLASH_PAGE_SIZE) || (len % SPI_FLASH_PAGE_SIZE))
    {
        printk("[%s]addr/len is not aligned\n", __func__);
        return -EINVAL;
    }

    addr_page = addr / SPI_FLASH_PAGE_SIZE;
    rlen = len / SPI_FLASH_PAGE_SIZE;

    for (i = 0; i < rlen; i++) {
        printk("page = %d\n", addr_page);
        SPIFlashPageRead(addr_page);
        addr_page += 1;

        SPIFlashRead(0x0, buf, SPI_FLASH_PAGE_SIZE);
        printk("1 = %d\n", buf[1]);
        buf += SPI_FLASH_PAGE_SIZE;
    }

    *retlen = len;
    return 0;
}

void SPIFlashProgram(unsigned int addr, unsigned char *buf, int len)
{
    unsigned char tx_buf[3];   
    struct spi_transfer	t[] = {
            {
                .tx_buf		= tx_buf,
                .len		= 3,
            },
            {
                .tx_buf		= buf,
                .len		= 2112,
            },
        };
    struct spi_message	m;

    tx_buf[0] = 0x02;
    tx_buf[1] = addr >> 8;
    tx_buf[2] = addr & 0xff;

    SPIFlashWriteEnable(1);

    spi_message_init(&m);
    spi_message_add_tail(&t[0], &m);
    spi_message_add_tail(&t[1], &m);
    spi_sync(hi_spi, &m);

    SPIFlashWaitWhenBusy();
}

void SPIFlashProgramExecute(unsigned int addr)
{
    unsigned char tx_buf[4];

    tx_buf[0] = 0x10;
    tx_buf[1] = 0x00;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xff;

    spi_write(hi_spi, tx_buf, 4);

    SPIFlashWaitWhenBusy();
}

static int my_flash_write(struct mtd_info *mtd, loff_t to, size_t len,
        size_t *retlen, const u_char *buf)
{
    unsigned int addr = to;
    unsigned int wlen  = 0;
    unsigned int addr_page = 0;
    int i = 0;
#if 1
    /* 判断参数 */
    printk("addr = %d.len = %d\n", addr, len);
    if ((addr % SPI_FLASH_PAGE_SIZE) || (len % SPI_FLASH_PAGE_SIZE))
    {
        printk("[%s]addr/len is not aligned\n", __func__);
        return -EINVAL;
    }
#endif
    addr_page = addr / SPI_FLASH_PAGE_SIZE;
    wlen = len / SPI_FLASH_PAGE_SIZE;

    for (i = 0; i < wlen; i++)
    {
        SPIFlashProgram(0x00, (unsigned char *)buf, SPI_FLASH_PAGE_SIZE); 
        printk("1 = %d. 5 = %d\n", buf[1], buf[5]);
        buf += SPI_FLASH_PAGE_SIZE;

        SPIFlashProgramExecute(addr_page);
        addr_page += 1;
    }

    *retlen = len;
    return (((readReg(0xc0) >> 3) & 0x1) == 0 ? 0 : -1);
}

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
    status = 0;

    SPIFlashReadID();
    //SPIChangeReg1Buf();
    //SPIChangeBuf();
    writeReg(0xa0, 0x00);
    //readReg(0xa0);
    //readReg(0xb0);
    //readReg(0xc0);

    //构造mtd_info
    memset(&spi_flash_dev, 0, sizeof(spi_flash_dev));
    spi_flash_dev.name = "yusense_spi_flash";
    spi_flash_dev.type = MTD_NANDFLASH;
    spi_flash_dev.flags = MTD_CAP_NANDFLASH;
    spi_flash_dev.size = 0x8000000;  /* 128M */
    spi_flash_dev.writesize = SPI_FLASH_PAGE_SIZE;
    spi_flash_dev.writebufsize = SPI_FLASH_PAGE_SIZE;
    spi_flash_dev.erasesize = 1024 * SPI_FLASH_BLOCK_SIZE;

    spi_flash_dev.owner = THIS_MODULE;
    spi_flash_dev._erase = my_flash_erase;
    spi_flash_dev._read  = my_flash_read;
    spi_flash_dev._write = my_flash_write;

    mtd_device_register(&spi_flash_dev, NULL, 0);

end1:
    put_device(d);
    
end0:
    return status;
}
static void __exit w25n01gw_exit(void)
{
    
}

module_init(w25n01gw_init);
module_exit(w25n01gw_exit);

MODULE_LICENSE("GPL");
