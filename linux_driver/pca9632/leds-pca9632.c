/******************************************************************************

         版权所有 (C), 2004-2025, 长光禹辰信息技术与装备(青岛)有限公司

 ******************************************************************************
  文 件 名   : leds-pca9632.c
  版 本 号   : 初稿
  作    者   : wzh
  生成日期   : 2018年8月11日, 星期六
  最近修改   :
  功能描述   : led灯驱动芯片驱动
  函数列表   :
              pca9632_config
              pca9632_exit
              pca9632_init
              pca9632_misc_ioctrl
              pca9632_set_blinking_state
              pca9632_set_brigntness
              pca9632_set_dmblnk
              pca9632_set_output_state
              pca9632_set_work_mode
  修改历史   :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 创建文件

******************************************************************************/

/*----------------------------------------------*
 * 包含头文件                                   *
 *----------------------------------------------*/
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/input.h>
#include <linux/leds-pca9532.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include "leds-pca9632.h"
/*----------------------------------------------*
 * 外部变量说明                                 *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 外部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 全局变量                                     *
 *----------------------------------------------*/
struct i2c_client* pca9632_client;
/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/
#define PCA9632_I2C    11        //i2c序号
#define MAJOR_PCA9632 200        //设备从地址
#define BRIGHTNESS_MIN 0         //亮度最小值
#define BRIGHTNESS_MAX 255       //亮度最大值
#define FREQ_MIN       9         //闪烁频率最小值
#define FREQ_6HZ       600       //闪烁频率6hz
#define FREQ_MAX       2400      //闪烁频率最大值
#define I2C_DEV_ADDR   0x62      //i2c从设备地址


static struct i2c_board_info pca9632_info =
{
    I2C_BOARD_INFO("pca9632", I2C_DEV_ADDR),
};

/* pca9632寄存器 00h-0ch */
enum pca9632_registers {
    REGISTER_MODE1 = 0x0,
    REGISTER_MODE2,
    REGISTER_PWM0,
    REGISTER_PWM1,
    REGISTER_PWM2,
    REGISTER_PWM3,
    REGISTER_GRPPWM,
    REGISTER_GRPFREQ,
    REGISTER_LEDOUT,
    REGISTER_SUBADR1,
    REGISTER_SUBADR2,
    REGISTER_SUBADR3,
    REGISTER_ALLCALLADR,
};

/* pca9632的工作模式，1、正常模式 2、低功耗模式 */
typedef enum pca9632_work_mode {
    MODE_NORMAL,
    MODE_SLEEP,
} PCA9632_WORK_MODE;

typedef enum pca9632_dmblnk {
    IS_BLINKING,
    IS_DIMMING,
} PCA9632_DMBINK;

typedef enum pca9632_channel {
    CHANNEL_RED,
    CHANNEL_GREEN,
    CHANNEL_BLUE,
    CHANNEL_ALL,
} PCA9632_CHANNEL;

typedef enum pca9632_output_state {
    STATE_OPEN  = 0x3,
    STATE_CLOSE = 0x0,
} PCA9632_OUTPUT_STATE;

/*****************************************************************************
 函 数 名  : pca9632_set_work_mode
 功能描述  : 设置pca9632工作模式
 输入参数  : PCA9632_WORK_MODE mode  
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_set_work_mode(PCA9632_WORK_MODE mode)
{
    int ret = -1;
    int val = 0;
    
    val = i2c_smbus_read_byte_data(pca9632_client, REGISTER_MODE1);
    if(MODE_NORMAL == mode)
    {
        ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_MODE1, val&(~BIT(4)));
    }
    else if(MODE_SLEEP == mode)
    {
        ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_MODE1, val|BIT(4));
    }
    else
    {
        printk(KERN_ERR "don't have this mode!\n");
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : pca9632_set_dmblnk
 功能描述  : 设置是否闪烁
 输入参数  : PCA9632_DMBINK dmblnk  
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_set_dmblnk(PCA9632_DMBINK dmblnk)
{
    int ret = -1;
    int val = 0;

    val = i2c_smbus_read_byte_data(pca9632_client, REGISTER_MODE2);
    switch(dmblnk)
    {
        case IS_BLINKING:
            ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_MODE2, val|BIT(5));
            break;
        case IS_DIMMING:
            ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_MODE2, val&(~BIT(5)));
            break;
        default:
            break;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : pca9632_set_brigntness
 功能描述  : 设置各个LED灯(R/G/B)亮度, 亮度共256个梯度，0:min 255:max
 输入参数  : PCA9632_CHANNEL channel  
             int brightness           
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_set_brigntness(PCA9632_CHANNEL channel, int brightness)
{
    int ret = -1;
    if((brightness < BRIGHTNESS_MIN)||(brightness > BRIGHTNESS_MAX))
    {
        goto EXIT;
    }

    switch(channel)
    {
        case CHANNEL_RED:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM0, brightness);
            break;
        case CHANNEL_GREEN:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM1, brightness);
            break;
        case CHANNEL_BLUE:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM2, brightness);
            break;
        case CHANNEL_ALL:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM0, brightness);
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM1, brightness);
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_PWM2, brightness);
            break;
        default:
            break;
    }
    
EXIT:
    return ret;
}

/*****************************************************************************
 函 数 名  : pca9632_set_blinking_state
 功能描述  : 设置灯闪烁的占空比和频率
 输入参数  : int duty_cycle  占空比(例:设置占空比为50%，该参数输入50即可)
             int freq        闪烁频率(扩大100倍，为避免进行浮点运算)
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_set_blinking_state(int duty_cycle, int freq)
{
    int ret = -1;
    int grpfreq = 0;
    int grppwm_duty_cycle = 0;

    if((freq < FREQ_MIN) || (freq > FREQ_MAX))
    {
        goto EXIT;
    }

    //计算写入寄存器06h的值
    if((freq > FREQ_MIN) && (freq < FREQ_6HZ))
    {
        grppwm_duty_cycle = (duty_cycle * 256 / 100)&(~BIT(0))&(~BIT(1));
        ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_GRPPWM, grppwm_duty_cycle);
        if(ret < 0)
            goto EXIT;
    }
    else
    {
        grppwm_duty_cycle = duty_cycle * 256 / 100;
        ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_GRPPWM, grppwm_duty_cycle);
        if(ret < 0)
            goto EXIT;
    }
    
    //计算写入寄存器07h的值
    grpfreq = 2400 / freq - 1;     
    //printk(KERN_ERR "GRPFREQ val=%d(wzh)\n", grpfreq);
    ret = i2c_smbus_write_byte_data(pca9632_client, REGISTER_GRPFREQ, grpfreq);
    
EXIT:
    return ret;
}

/*****************************************************************************
 函 数 名  : pca9632_set_output_state
 功能描述  : 设置各个通道的开关状态
 输入参数  : PCA9632_CHANNEL channel     通道   
             PCA9632_OUTPUT_STATE state  状态
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_set_output_state(PCA9632_CHANNEL channel, PCA9632_OUTPUT_STATE state)
{
    int ret = -1;
    int val = 0;

    val = i2c_smbus_read_byte_data(pca9632_client, REGISTER_LEDOUT);
    switch(channel)
    {
        case CHANNEL_RED:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_LEDOUT, val|(state << 4));
            break;
        case CHANNEL_GREEN:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_LEDOUT, val|(state << 2));
            break;
        case CHANNEL_BLUE:
            i2c_smbus_write_byte_data(pca9632_client, REGISTER_LEDOUT, val|(state << 0));
            break;
        case CHANNEL_ALL:
            if(STATE_OPEN == state)
                i2c_smbus_write_byte_data(pca9632_client, REGISTER_LEDOUT, 0xff);
            else
                i2c_smbus_write_byte_data(pca9632_client, REGISTER_LEDOUT, 0x0);
            break;
        default:
            break;
    }

    return ret;
}


/*****************************************************************************
 函 数 名  : pca9632_get_output_state
 功能描述  : 设置各个通道的开关状态
 输入参数  : PCA9632_CHANNEL channel     通道   
             
 输出参数  : 无
 返 回 值  : 
 
*****************************************************************************/
int pca9632_get_output_state(void)
{
    int ret = 0;
    int val = 0;

    val = i2c_smbus_read_byte_data(pca9632_client, REGISTER_LEDOUT);
    ret = val;
    return ret;
}


/*****************************************************************************
 函 数 名  : pca9632_config
 功能描述  : pca9632初始化配置
 输入参数  : struct i2c_client *client  
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
int pca9632_config(struct i2c_client *client)
{
    int ret = -1;

    //设置pca9632工作在normal模式
    pca9632_set_work_mode(MODE_NORMAL);

    //设置闪烁
    pca9632_set_dmblnk(IS_BLINKING);

    //设置LED灯亮度
    pca9632_set_brigntness(CHANNEL_ALL, 128);

#if 0
    pca9632_set_blinking_state(50, 100);
    pca9632_set_output_state(CHANNEL_RED, STATE_OPEN);
#endif
    return ret;
}

/*****************************************************************************
 函 数 名  : pca9632_misc_ioctrl
 功能描述  : pca9632 ioctrl
 输入参数  : struct file *file  文件
             unsigned int cmd   命令
             unsigned long val  闪烁频率(扩大100倍，例:设置为1hz，该值为100)
 输出参数  : 无
 返 回 值  : 
 
 修改历史      :
  1.日    期   : 2018年8月11日, 星期六
    作    者   : wzh
    修改内容   : 新生成函数

*****************************************************************************/
long pca9632_misc_ioctrl(struct file *file, unsigned int cmd, unsigned long val)
{
    long ret = 0;
    int value = 0;

    //先把灯全部关闭，再打开想开启的灯
    if(LED_STATUS != cmd)
        pca9632_set_output_state(CHANNEL_ALL, STATE_CLOSE);
	
	//printk(KERN_DEBUG "cmd=%d,val=%ld\n", cmd, val);
	pca9632_set_dmblnk(IS_BLINKING);
	pca9632_set_blinking_state(50, val);
	
    
    switch(cmd)
    {
        case LED_RED:
            pca9632_set_output_state(CHANNEL_RED, STATE_OPEN);
            break;
        case LED_GREEN:
            pca9632_set_output_state(CHANNEL_GREEN, STATE_OPEN);
            break;
        case LED_BLUE:
            pca9632_set_output_state(CHANNEL_BLUE, STATE_OPEN);
            break;
        case LED_YELLO:
            pca9632_set_output_state(CHANNEL_RED, STATE_OPEN);
            pca9632_set_output_state(CHANNEL_GREEN, STATE_OPEN);
            break;
        case LED_CYAN:
            pca9632_set_output_state(CHANNEL_GREEN, STATE_OPEN);
            pca9632_set_output_state(CHANNEL_BLUE, STATE_OPEN);
            break;
        case LED_MAGENTA:
            pca9632_set_output_state(CHANNEL_RED, STATE_OPEN);
            pca9632_set_output_state(CHANNEL_BLUE, STATE_OPEN);
            break;
        case LED_WHITE:
            pca9632_set_output_state(CHANNEL_ALL, STATE_OPEN);
            break;
        case LED_STATUS:
            value = pca9632_get_output_state();
            //printk("leds driver output status = %x \n",value);
            copy_to_user(val, &value, sizeof(value));
            break;
		case LED_RED_ALWAYS:
			pca9632_set_dmblnk(IS_DIMMING);
			pca9632_set_output_state(CHANNEL_RED, STATE_OPEN);
			break;
		case LED_GREEN_ALWAYS:
			pca9632_set_dmblnk(IS_DIMMING);
			pca9632_set_output_state(CHANNEL_GREEN, STATE_OPEN);
			break;
		case LED_BLUE_ALWAYS:
			pca9632_set_dmblnk(IS_DIMMING);
			pca9632_set_output_state(CHANNEL_BLUE, STATE_OPEN);
			break;
		case LED_TURN_OFF:
			pca9632_set_dmblnk(IS_DIMMING);
			pca9632_set_output_state(CHANNEL_ALL, STATE_CLOSE);
			break;
        default:
            break;
    }
    
    return ret;
}

struct file_operations pca9632_misc_ops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = pca9632_misc_ioctrl,
};

struct miscdevice pca9632_misc_dev = {
    .minor = MAJOR_PCA9632,
    .name  = "pca9632",
    .fops  = &pca9632_misc_ops,
};

static int __init pca9632_init(void)
{
    int ret;
    struct i2c_adapter* i2c_adap=NULL;

    //初始化I2C
    i2c_adap = i2c_get_adapter(PCA9632_I2C);
    pca9632_client = i2c_new_device(i2c_adap, &pca9632_info);

    pca9632_config(pca9632_client);

    ret = misc_register(&pca9632_misc_dev);
    if(ret < 0)
        misc_deregister(&pca9632_misc_dev);
    
    return ret;
}

static void __exit pca9632_exit(void)
{
	misc_deregister(&pca9632_misc_dev);
}

MODULE_AUTHOR("Hislicon");
MODULE_LICENSE("GPL");

module_init(pca9632_init);
module_exit(pca9632_exit);

