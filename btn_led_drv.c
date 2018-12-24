/*
 * @brief   : this is simple driver maily used to control button and led on smart210 board . it's expose a device file to /dev in user space
 *  	    : i will guide how to write driver to expose interface to user via sys/class in another driver .  
 *          : and also demo how to use timer and interrupt in kernel space 
 * @author  : giann <ngocgia73@gmail.com>
 * @filename: button_led_drv.c
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h> 
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/wait.h>



#define DEVICE_NAME			"btn_pd"

// use datasheet to know exactly these register 
#define PORTH2_BASE_ADDR		0xE0200C40
#define PORTH3_BASE_ADDR        	0xE0200C60
#define PORTJ2_BASE_ADDR		0xE0200280

#define CMD_LED_ON	1
#define CMD_LED_OFF 	0
#define LED_NUM 	4

// names used to display in /sys/class
const char * class_name = "btn_sc";

struct button_desc {
	int irq;
	int number;
	char *name;
	struct timer_list timer;
};

struct button_desc buttons[] = {
	{ IRQ_EINT(16), 0, "KEY0" },
	{ IRQ_EINT(17), 1, "KEY1" },
	{ IRQ_EINT(18), 2, "KEY2" },
	{ IRQ_EINT(19), 3, "KEY3" },
	{ IRQ_EINT(24), 4, "KEY4" },
	{ IRQ_EINT(25), 5, "KEY5" },
	{ IRQ_EINT(26), 6, "KEY6" },
	{ IRQ_EINT(27), 7, "KEY7" },
};

// struct GPIO PORT
typedef struct gpio_port    
{
       	u32	con;		// Configuration Register
        u32	dat;		// Data Register
        u32	pud;		// Pull-up/down Register
        u32	drv;		// Drive Strength Control Register     
} GPIO_PORT;
GPIO_PORT *PortH2,*PortH3,*PortJ2;

static volatile char key_values[] = {
	'0', '0', '0', '0', '0', '0', '0', '0'
};

static DECLARE_WAIT_QUEUE_HEAD(my_waitq);

static volatile int ev_press = 0;

// function handle timer interupt
static void smart210_buttons_leds_timer(unsigned long _data)
{
	int state;
	int down;
	struct button_desc *bdata = (struct button_desc *)_data;
	// get value of button
	// we just want to read first 4 buttons
	if ((bdata ->number) < 4) {
		state = ((PortH2 ->dat) >> (bdata ->number)) & 1U;
		printk ("\n===%s => %d===\n",bdata ->name,state);
		// make sure when the button is really change => need update to user
		down = !state;
		if (down != (key_values[bdata->number] & 1)) {
			key_values[bdata ->number] = '0' + down;

			ev_press = 1;
			// signal to user to read new data 
			wake_up_interruptible(&my_waitq);
		}
	}
}

// this function will be called if cpu got interrupt signal
static irqreturn_t button_interrupt(int irq, void *dev_id)
{
	struct button_desc *bdata = (struct button_desc *)dev_id;
	printk("\n===%s trigger=== \n",bdata->name);
	// funtion to modify expire value of timer
	// smart210_buttons_leds_timer function will be called after 50ms
	mod_timer (&bdata->timer,jiffies + msecs_to_jiffies(50));
	return IRQ_HANDLED;
}

static int smart210_buttons_leds_open(struct inode *inode, struct file *file)
{
	int i;
	int irq;
	int err;
	// init GPH2
	// init GPIOH2 input
	PortH2 ->con = 0x00000000;
	// enable pull up 
	PortH2 ->pud = 0xAAAA;
	// init GPH3
	// init GPIOH2 input
	PortH3 ->con = 0x00000000;
	// enable pull up 
	PortH3 ->pud = 0xAAAA;
	// init GPIOJ2 as output
	PortJ2 ->con = 0x11111111;
	// init external interupt 
	for (i = 0; i < sizeof(buttons) / sizeof(struct button_desc); i++)
	{
		setup_timer(&buttons[i].timer,smart210_buttons_leds_timer,(unsigned long)&buttons[i]);
		irq = buttons[i].irq;
		// IRQF_TRIGGER_FALLING
		// IRQ_TYPE_EDGE_BOTH
		err = request_irq(irq, button_interrupt, IRQ_TYPE_EDGE_BOTH, 
				buttons[i].name, (void *)&buttons[i]);
		if(err)
			break;
	}

	if(err)
	{
		i--;
		for (; i >= 0; i--)
		{
			irq = buttons[i].irq;
			free_irq(irq,(void *)&buttons[i]);
		}
		return -EBUSY;
	}
	return 0;
}

static int smart210_buttons_leds_close(struct inode *inode, struct file *file)
{
	int i;
	int irq;
  	for (i = 0; i < sizeof(buttons) / sizeof(struct button_desc); i++)
	{
		irq = buttons[i].irq;
		free_irq(irq,(void *)&buttons[i]);
		del_timer_sync(&buttons[i].timer);
	}
	return 0;
}

static int smart210_buttons_leds_read(struct file *filp, char __user *buff,
		size_t count, loff_t *offp)
{
	int ret;
	if (!ev_press)
	{
 		// wait here until we have new data. avoid busy waiting  
		wait_event_interruptible(my_waitq, ev_press);
	} 
	ev_press = 0;
	ret = copy_to_user((void *)buff, (const void *)(&key_values),
			min(sizeof(key_values), count));
	return ret;
}

static long smart210_buttons_leds_ioctl(struct file *filp, unsigned int cmd,
	   unsigned long arg)
{
	// smart210 board have 4 leds.
	char number_of_leds = (char)arg;
	switch (cmd){
		case CMD_LED_ON:
			if (arg > LED_NUM)
				return -EINVAL;
			// turn on leds 
			PortJ2->dat &= (~(1 << number_of_leds));
			break;
		case CMD_LED_OFF:
			if (arg > LED_NUM)
				return -EINVAL;
			// turn off leds
			PortJ2->dat |= (1 << number_of_leds);
			break;
		default:
			return	-EINVAL;
	}
	return 0;	
}

static struct file_operations dev_fops = {
	.owner			= THIS_MODULE,
	.open			= smart210_buttons_leds_open,
	.release		= smart210_buttons_leds_close, 
	.read			= smart210_buttons_leds_read,
	.unlocked_ioctl 	= smart210_buttons_leds_ioctl,
};

// define for register with dynamic allocate 
static dev_t _dev_t;
static struct cdev *c_dev;
static struct class *_class;

static int __init button_dev_init(void)
{
	int ret = 0;
	printk("welcome btn_led driver\n");

	PortH2 = ioremap(PORTH2_BASE_ADDR,sizeof(GPIO_PORT));
	if (PortH2 == NULL) {
		printk("Failed to remap register block\n");
		return -1;
	}

	PortJ2 = ioremap(PORTJ2_BASE_ADDR,sizeof(GPIO_PORT));
	if (PortJ2 == NULL)
	{
		printk("Failed to remap register block\n");
		return -1;
	}

	PortH3 = ioremap(PORTH3_BASE_ADDR,sizeof(GPIO_PORT));
	if (PortH3 == NULL) {
		printk("Failed to remap register block\n");
		return -1;
	}

	// register driver with dynamic allocate
	ret = alloc_chrdev_region(&_dev_t, 0, 1, DEVICE_NAME);
	if (ret < 0)
	{
		printk (" Failed to alloc_chrdev_region\n");
		return -3;
	}

	c_dev = cdev_alloc();
	if (c_dev == NULL)
	{
		goto __CDEV_ERR;
	}

	cdev_init(c_dev, &dev_fops);
	ret = cdev_add(c_dev, _dev_t, 1);
	if (ret < 0)
	{
		goto __CDEV_ERR;
	}

	_class = class_create(THIS_MODULE, class_name);   //$ls /sys/class
	if (_class == NULL)
	{
		goto __CLASS_CREATE_ERR;
	}

	if(device_create(_class, NULL, _dev_t, NULL, "btn_d%d",MINOR(_dev_t)) == NULL) //ls /dev/
	{
		goto __DEVICE_CREATE_ERR;
	}
	printk ("Registered character driver by alloc_chrdev_region with major %d minor %d ok\n",MAJOR(_dev_t),MINOR(_dev_t));	
	return ret;

__CDEV_ERR:
	unregister_chrdev_region(_dev_t, 1);
	return -ENOMEM;
__CLASS_CREATE_ERR:
	cdev_del(c_dev);
	unregister_chrdev_region(_dev_t, 1);
	return -EEXIST;
__DEVICE_CREATE_ERR:
	class_destroy(_class);
	cdev_del(c_dev);
	unregister_chrdev_region(_dev_t, 1);
	return -EINVAL;;
	

}

static void __exit button_dev_exit(void)
{
	iounmap(PortH2);
	iounmap(PortH3);
	iounmap(PortJ2);
	// unregister 
	device_destroy(_class, _dev_t);
	class_destroy(_class);
	cdev_del(c_dev);
	unregister_chrdev_region(_dev_t, 1);
	printk("removed btn_led driver\n");
}

module_init(button_dev_init);
module_exit(button_dev_exit);

MODULE_AUTHOR("Ngoc Gia <ngocgia73@gmail.com>");
MODULE_DESCRIPTION("sample device driver");
MODULE_LICENSE("GPL");

