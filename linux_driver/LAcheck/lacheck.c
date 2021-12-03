#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/of_gpio.h>

#define LACHECK_DEV_CNT           1
#define LACHECK_DEV_NAME          "lacheckdev"

static struct fasync_struct *lacheck_async;

typedef struct {
    dev_t               lacheck_id;
    struct cdev         lacheck_cdev;
    struct class       *lacheck_class;
    struct device      *lacheck_device;
    int                 lacheck_major;
    struct device_node *lacheck_node;
    int                 lacheck_gpio;
    int                 lacheck_irq_num;
} LACHECK_DEV;

LACHECK_DEV lacheck_dev;   //lacheck设备

static int lacheck_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t lacheck_write(struct file *filp,const char __user *buf,size_t cnt, loff_t *offt)
{
    return 0;
}

static int lacheck_fasync(int fd, struct file *filp, int on)
{
    return fasync_helper(fd, filp, on, &lacheck_async);
}

static irqreturn_t lacheck_irq_handler(int irq, void *dev_id)
{
    printk(KERN_ERR"lacheck irq is start!\n");
    kill_fasync (&lacheck_async, SIGIO, POLL_IN);
    return IRQ_HANDLED;
}

static struct file_operations lacheck_fops = {
    .owner = THIS_MODULE,
    .open = lacheck_open,
    .write = lacheck_write,
    .fasync = lacheck_fasync,
};

static int lacheck_probe(struct platform_device *dev)
{
    int ret = 0;
    char lacheck_gpio_name[16] = "lacheck_gpio";

    printk(KERN_ERR"probe is OK!\n");

    //创建设备号
    if(lacheck_dev.lacheck_major) {
        lacheck_dev.lacheck_id = MKDEV(lacheck_dev.lacheck_major, 0);
        register_chrdev_region(lacheck_dev.lacheck_id, LACHECK_DEV_CNT, LACHECK_DEV_NAME);
    }
    else {
        alloc_chrdev_region(&lacheck_dev.lacheck_id, 0, LACHECK_DEV_CNT, LACHECK_DEV_NAME);//申请设备号
        lacheck_dev.lacheck_major = MAJOR(lacheck_dev.lacheck_id);//获取主设备号
    }

    //初始化cdev
    lacheck_dev.lacheck_cdev.owner = THIS_MODULE;
    cdev_init(&lacheck_dev.lacheck_cdev, &lacheck_fops);

    cdev_add(&lacheck_dev.lacheck_cdev, lacheck_dev.lacheck_id, LACHECK_DEV_CNT);

    lacheck_dev.lacheck_class = class_create(THIS_MODULE, LACHECK_DEV_NAME);
    if(IS_ERR(lacheck_dev.lacheck_class))
    {
        return PTR_ERR(lacheck_dev.lacheck_class);
    }

    lacheck_dev.lacheck_device = device_create(lacheck_dev.lacheck_class, NULL,
        lacheck_dev.lacheck_id, NULL, LACHECK_DEV_NAME);
    if(IS_ERR(lacheck_dev.lacheck_device))
    {
        return PTR_ERR(lacheck_dev.lacheck_device);
    }

    lacheck_dev.lacheck_node = of_find_node_by_path("/la_irq");
    if(lacheck_dev.lacheck_node == NULL)
    {
        printk("la_irq node not find!\r\n");
        return -EINVAL;
    }

    lacheck_dev.lacheck_gpio = of_get_named_gpio(lacheck_dev.lacheck_node, "la-gpio", 0);
    if(lacheck_dev.lacheck_gpio < 0)
    {
        printk("can not get la-gpio!\r\n");
        return -EINVAL;
    }

    gpio_request(lacheck_dev.lacheck_gpio, lacheck_gpio_name);
    lacheck_dev.lacheck_irq_num = gpio_to_irq(lacheck_dev.lacheck_gpio);

    ret = request_irq(lacheck_dev.lacheck_irq_num,
                        lacheck_irq_handler,
                        IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
                        "lacheck_irq",
                        NULL);
    if(ret) {
		printk(KERN_ERR "can not get irq\n");
		return ret;
    }

    return ret;
}

static int lacheck_remove(struct platform_device *dev)
{
    return 0;
}

static const struct of_device_id lacheck_of_match[] = 
{
    {   .compatible = "lacheck"  },
    {}
};

static struct platform_driver lacheck_driver = {
    .driver = {
        .name = "lacheck",
        .of_match_table = lacheck_of_match,
    },
    .probe = lacheck_probe,
    .remove = lacheck_remove,
};

static int __init lacheck_init(void)
{
    return platform_driver_register(&lacheck_driver);
}

static void __exit lacheck_exit(void)
{
    platform_driver_unregister(&lacheck_driver);
}

module_init(lacheck_init);
module_exit(lacheck_exit);

MODULE_LICENSE("GPL");    
MODULE_AUTHOR("hantek");
