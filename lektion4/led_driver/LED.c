#include <linux/gpio.h> 
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <linux/device.h>
 #include <linux/uaccess.h>
 #include <linux/module.h>

#define GPIO_NR 21
const int first_minor = 0;
const int max_devices = 255;
static dev_t devno;
static struct class *LED_class;
static struct cdev LED_cdev;

int LED_open(struct inode* inode, struct file* filep){
    int major, minor;
    major = MAJOR(inode->i_rdev);
    minor = MINOR(inode->i_rdev);
    printk("Opening MyGpio Device [major], [minor]: %i, %i\n", major, minor);
    return 0;
}

int LED_release(struct inode* inode, struct file* filep){
    int minor, major;
    major = MAJOR(inode->i_rdev);
    minor = MINOR(inode->i_rdev);
    printk("Closing/Releasing MyGpio Device [major], [minor]: %i, %i\n", major, minor);
    return 0;
}

ssize_t LED_read(struct file* filep, char __user *buf, size_t count, loff_t* f_pos){
    char kbuf[12];
    int len, value;
    value = gpio_get_value(GPIO_NR);
    len = count < 12 ? count : 12;
    len = snprintf(kbuf, len, "%i", value);
    unsigned long err = copy_to_user(buf, kbuf, ++len);
    if(err) return -1;
    *f_pos += len;
    return len;
}

ssize_t LED_write(struct file *filep, const char __user *ubuf, size_t count, loff_t *f_pos){
    int len, value, err = 0;
    char kbuf[12];
    len = count < 12 ? count : 12;
    err = copy_from_user(kbuf, ubuf, len); 
    if(err) return -EFAULT;
    kbuf[len] = 0;
    err = kstrtoint(kbuf, 0, &value);
    if(err) return -EFAULT;
    gpio_set_value(GPIO_NR, value);
    *f_pos += len;
    return len;
}

struct file_operations LED_fops ={
    .owner = THIS_MODULE,
    .open = LED_open,
    .release = LED_release,
    .read = LED_read,
    .write = LED_write
};

static int __init mygpio_init(void)
{
 // Request GPIO
devno = gpio_request(GPIO_NR, "16");

 // Set GPIO direction (in or out)
gpio_direction_output(GPIO_NR, 0); // output, default slukket

 // Alloker Major/Minor
int err=0;
err = alloc_chrdev_region(&devno, first_minor, max_devices, "LED-driver");
if(MAJOR(devno) <= 0)
pr_err("Failed to register chardev\n");
pr_info("LED-Driver got Major %i\n", MAJOR(devno));

 // Class Create 
LED_class = class_create(THIS_MODULE, "LED-class");
if (IS_ERR(LED_class)) pr_err("Failed to create class");

 // Cdev Init
 cdev_init(&LED_cdev, &LED_fops);

 // Add Cdev
 err = cdev_add(&LED_cdev, devno, 255);
if (err) pr_err("Failed to add cdev");
return err;
}

 static void __exit mygpio_exit(void)
 {
 // Delete Cdev
cdev_del(&LED_cdev);
 // Unregister Device
unregister_chrdev_region(devno, max_devices);
 // Class Destroy
 class_destroy(LED_class);
 // Free GPIO
 gpio_free(GPIO_NR);
 }

module_init(mygpio_init);
module_exit(mygpio_exit);
MODULE_AUTHOR("Karl");
MODULE_LICENSE("GPL");
