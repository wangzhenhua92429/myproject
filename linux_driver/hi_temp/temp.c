#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/io.h>

#define SYS_WRITEL(Addr, Value) ((*(volatile unsigned int *)(Addr)) = (Value))
#define SYS_READ(Addr)          (*((volatile int *)(Addr)))

static void* tsensor_ctl_base;
static void* tsensor_val_base;

static long temp_ioctl(struct file *pdbe_file, unsigned int dbe_cmd, unsigned long dbe_arg)
{
    unsigned int val;
    
    val = SYS_READ((volatile int *)tsensor_val_base);
    val &= 0x3ff;
    
    copy_to_user(dbe_arg, &val, sizeof(val));
}

static const struct file_operations temp_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = temp_ioctl,
};

static struct miscdevice temp_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "temp",
    .fops = &temp_fops,
};

static int __init temp_init(void)
{
    unsigned long val = 0;
    
    tsensor_ctl_base = (void *)ioremap(0x12030070, 8);
    if(NULL == tsensor_ctl_base)
    {
        printk("0x12030070 ioremap error!\n");
        return -1;
    }
    
    tsensor_val_base = (void *)ioremap(0x12030078, 8);
    if(NULL == tsensor_val_base)
    {
        printk("0x12030078 ioremap error!\n");
        return -1;
    }
    
    val = SYS_READ((volatile int *)tsensor_ctl_base);
    val |= (1 << 31) | (5 << 20);
    SYS_WRITEL((volatile unsigned int *)tsensor_ctl_base, val);
    
    return misc_register(&temp_dev);
}

static void __exit temp_exit(void)
{
    iounmap(tsensor_ctl_base);
    iounmap(tsensor_val_base);
    misc_deregister(&temp_dev);
}
module_init(temp_init);
module_exit(temp_exit);
MODULE_AUTHOR("Hislicon");
MODULE_LICENSE("GPL");