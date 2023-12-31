#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>


unsigned int GPIO_NR = 20;
const int first_minor = 0;
const int max_devices = 255;
static dev_t devno;
static struct class *LED_class;
static struct cdev LED_cdev;
static struct device *LED_device;

static int LED_probe(struct platform_device *pdev){

    printk(KERN_DEBUG "New Platform device: %s\n", pdev->name);
    printk("hello from probe");
    /* Request resources */
    gpio_request(GPIO_NR, "led");
    /* Dynamically add device */
    LED_device = device_create(LED_class, NULL, MKDEV(MAJOR(devno), first_minor), NULL, "mygpio%d", GPIO_NR);
    

    return 0;}

static int LED_remove(struct platform_device *pdev)
{
    printk (KERN_ALERT "Removing device %s\n", pdev->name);
    printk("hello from remove");
    /* Remove device created in probe, this must be
    * done for all devices created in probe */
    device_destroy(LED_class,
    MKDEV(MAJOR(devno), first_minor));
    gpio_free(GPIO_NR);
    
    return 0;
}


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

static const struct of_device_id my_led_platform_device_match[] = {
{ .compatible = "ase, plat_drv",}, {},
};

static struct platform_driver my_led_platform_driver = {
    .probe = LED_probe,
    .remove = LED_remove,
    .driver = {
        .name = "led",
        .of_match_table = my_led_platform_device_match,
        .owner = THIS_MODULE, }, };

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

devno = gpio_request(GPIO_NR, "led");

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

platform_driver_register(&my_led_platform_driver);

 // Cdev Init
 cdev_init(&LED_cdev, &LED_fops);

 // Add Cdev
 err = cdev_add(&LED_cdev, devno, 255);
if (err) pr_err("Failed to add cdev");
return err;
}

 static void __exit mygpio_exit(void)
 {
platform_driver_unregister(&my_led_platform_driver);
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
