#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>


#define PPS_GPIO_NUM		103	//SCH: ARM_PPS, GPIO12_7
#define RESET_FPGA_GPIO_NUM 	98	//SCH: REST_SW_FPGA, GPIO12_2
#define TRIGGR_IN_GPIO_NUM 	2	//SCH: Co_GPIO_OUT, GPIO0_2
#define FLASH_OUT_GPIO_NUM 	1	//SCH: Co_GPIO_IN, GPIO0_1


/**
 * 模块参数，中断触发类型
 * 0 - disable irq
 * 1 - rising edge triggered
 * 2 - falling edge triggered
 * 3 - rising and falling edge triggred
 * 4 - high level triggered
 * 8 - low level triggered
 */
static unsigned int gpio_irq_type = 1;
module_param(gpio_irq_type, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_irq_type, "gpio irq type");

spinlock_t lock;

static uint32_t key_value = 0;
static struct fasync_struct *gpio_async;
static unsigned int irq_num[2];

static void flashOutPulse(void)
{
	gpio_set_value(FLASH_OUT_GPIO_NUM, 1);
	udelay(500);
	gpio_set_value(FLASH_OUT_GPIO_NUM, 0);
}

static long aq600_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
	{
		case 0: //reset imx265 sensor FPGA
			gpio_set_value(RESET_FPGA_GPIO_NUM, 0);
			break;
		case 1: //set imx265 sensor FPGA
			gpio_set_value(RESET_FPGA_GPIO_NUM, 1);
			break;
		case 2:
			flashOutPulse();
			break;
		default:
			gpio_set_value(RESET_FPGA_GPIO_NUM, 1);
			break;
	}
	
	
	return 0;
}

int gpio_fasync(int fd, struct file *file, int mode)
{
	return fasync_helper(fd, file, mode, &gpio_async);
}

ssize_t key_value_read(struct file *file, char __user *buf, size_t size, loff_t *ppof)
{
	int ret = -1;

    disable_irq(irq_num[0]);
    disable_irq(irq_num[1]);

	ret = copy_to_user(buf, &key_value, 4);
	if(0 != ret)
	{
		printk("copy_to_user error!\n");
	}

    key_value = 0;

    enable_irq(irq_num[1]);
    enable_irq(irq_num[0]);

    return ret;
}

static irqreturn_t trigger_in_gpio_isr(int irq, void *dev_id)
{
	key_value |= 0x01;
	kill_fasync(&gpio_async, SIGIO, POLL_IN);
    
	return IRQ_HANDLED;
}

static irqreturn_t pps_gpio_isr(int irq, void *dev_id)
{
	key_value |= 0x02;
	kill_fasync(&gpio_async, SIGIO, POLL_IN);
    
	return IRQ_HANDLED;
}

static void gpio_dev_irq_exit(unsigned int gpio_num)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	free_irq(gpio_to_irq(gpio_num), &gpio_irq_type);
	spin_unlock_irqrestore(&lock, flags);
}

static const struct file_operations gpio_fops = {
	.owner = THIS_MODULE,
	.read = key_value_read,
	.unlocked_ioctl = aq600_ioctl,
	.fasync = gpio_fasync,
};

static struct miscdevice tri_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gpio-ms40x",
    .fops = &gpio_fops,
};

static int __init gpio_aq600_init(void)
{
	unsigned int irqflags = 0;	
    uint32_t *regCtl;

    //引脚复用
    regCtl = ioremap(0x1F00106C, 4);
    *regCtl = 0x1140;
    iounmap(regCtl);

    regCtl = ioremap(0x1F001074, 4);
    *regCtl = 0x1140;
    iounmap(regCtl);

    regCtl = ioremap(0x1F001078, 4);
    *regCtl = 0x1140;
    iounmap(regCtl);

    regCtl = ioremap(0x1F001080, 4);
    *regCtl = 0x1140;
    iounmap(regCtl);

	gpio_request(RESET_FPGA_GPIO_NUM, NULL);
	gpio_direction_output(RESET_FPGA_GPIO_NUM, 0);
	
	gpio_request(FLASH_OUT_GPIO_NUM, NULL);
	gpio_direction_output(FLASH_OUT_GPIO_NUM, 0);

	gpio_request(TRIGGR_IN_GPIO_NUM, NULL);
	gpio_direction_input(TRIGGR_IN_GPIO_NUM);
    
	irqflags =  IRQF_TRIGGER_RISING;
	irqflags |= IRQF_SHARED;

	irq_num[0] = gpio_to_irq(TRIGGR_IN_GPIO_NUM);
	if (request_irq(irq_num[0], trigger_in_gpio_isr, irqflags, "trigger_in_gpio", &gpio_irq_type))
	{
		printk("[%s %d]request_irq error!\n", __func__, __LINE__);
		return -1;
	}
	
	gpio_request(PPS_GPIO_NUM, NULL);
	gpio_direction_input(PPS_GPIO_NUM);

	irq_num[1] = gpio_to_irq(PPS_GPIO_NUM);
	if (request_irq(irq_num[1], pps_gpio_isr, irqflags, "pps_gpio", &gpio_irq_type))
	{
		printk("[%s %d]request_irq error!\n", __func__, __LINE__);
		return -1;
	}    
	
	misc_register(&tri_dev);
	
	return 0;
}

static void __exit gpio_aq600_exit(void)
{
	//del_timer(&triTimer);

	gpio_dev_irq_exit(PPS_GPIO_NUM);
	gpio_dev_irq_exit(TRIGGR_IN_GPIO_NUM);

	gpio_free(PPS_GPIO_NUM);
	gpio_free(RESET_FPGA_GPIO_NUM);
	gpio_free(TRIGGR_IN_GPIO_NUM);
	gpio_free(FLASH_OUT_GPIO_NUM);

	misc_deregister(&tri_dev);
}

module_init(gpio_aq600_init);
module_exit(gpio_aq600_exit);
MODULE_AUTHOR("Yusense");
MODULE_LICENSE("GPL");
