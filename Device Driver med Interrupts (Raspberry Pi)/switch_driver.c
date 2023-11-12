#include <linux/gpio.h> 

 #include <linux/fs.h>

 #include <linux/cdev.h>

 #include <linux/device.h>

 #include <linux/uaccess.h>

 #include <linux/module.h>
 #include <linux/interrupt.h>
 #include <linux/wait.h>
 #include <linux/sched.h>

//Driver 1 SW2

#define GPIONR 19

const int first_minor = 0;

const int max_devices = 255;

static dev_t devno;

static struct class *switch_class;

static struct cdev switch_cdev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;

int isr_gpio_value, proc_gpio_value;

static irqreturn_t mygpio_isr(int irq, void *dev_id) {
  printk(KERN_INFO "Assigned IRQ line: %d\n", irq);
  flag = 1;
  wake_up_interruptible(&wq);
  isr_gpio_value = gpio_get_value(GPIONR);
  return IRQ_HANDLED;
}


int switch_open(struct inode* inode, struct file* filep){
    
    int major, minor;

    major = MAJOR(inode->i_rdev);

    minor = MINOR(inode->i_rdev);

    printk("Opening MyGpio Device [major], [minor]: %i, %i\n", major, minor);

    return 0;

}

int switch_release(struct inode* inode, struct file* filep){

    int minor, major;

    major = MAJOR(inode->i_rdev);

    minor = MINOR(inode->i_rdev);

    printk("Closing/Releasing MyGpio Device [major], [minor]: %i, %i\n", major, minor);

    return 0;

}

ssize_t switch_read(struct file* filep, char __user *buf, size_t count, loff_t* f_pos){

    wait_event_interruptible(wq, flag == 1);
    flag = 0;
    proc_gpio_value = gpio_get_value(GPIONR);

    char kbuf[12];

    int len, value;

    value = gpio_get_value(GPIONR);

    len = count < 12 ? count : 12;

    len = sprintf(buf, "%i %i", isr_gpio_value, proc_gpio_value);

    unsigned long err = copy_to_user(buf, kbuf, ++len);

    if(err) return -1;

    *f_pos += len;

    return len;

}

struct file_operations switch_fops ={

    .owner = THIS_MODULE,

    .open = switch_open,

    .release = switch_release,

    .read = switch_read

};

static int mygpio_init(void)

 { 

    gpio_request(GPIONR, "Req: sw2");

    gpio_direction_input(GPIONR); // output, default slukket

    int irq_result = request_irq(gpio_to_irq(GPIONR), mygpio_isr, IRQF_TRIGGER_FALLING,"mygpio_irq", NULL);
    if (irq_result) {
    printk(KERN_ERR "Error requesting IRQ: %d\n", irq_result);
    }
    
    int err=0;

    err = alloc_chrdev_region(&devno, first_minor, max_devices, "switch_driver");

    if(MAJOR(devno) <= 0)

    pr_err("Failed to register chardev\n");

    pr_info("Switch driver got Major %i\n", MAJOR(devno));

    switch_class = class_create(THIS_MODULE, "switch-class");

    if (IS_ERR(switch_class)) pr_err("Failed to create class");

    cdev_init(&switch_cdev, &switch_fops);

    err = cdev_add(&switch_cdev, devno, max_devices);

    if (err) pr_err("Failed to add cdev");

    return err;
 }

 static void mygpio_exit(void)

 {
   free_irq(gpio_to_irq(GPIONR), NULL);
 // Delete Cdev
    cdev_del(&switch_cdev);
 // Unregister Device
    unregister_chrdev_region(devno, max_devices);
 // Class Destroy
    class_destroy(switch_class);
    gpio_free(GPIONR);

 }

 module_init(mygpio_init);

 module_exit(mygpio_exit);

 MODULE_LICENSE("GPL");

 MODULE_AUTHOR("lol");
