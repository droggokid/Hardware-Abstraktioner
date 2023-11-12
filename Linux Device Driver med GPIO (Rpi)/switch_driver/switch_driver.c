#include <linux/gpio.h> 
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <linux/device.h>
 #include <linux/uaccess.h>
 #include <linux/module.h>

//Driver 1 SW2
#define GPIONR 16
const int first_minor = 0;
const int max_devices = 255;
static dev_t devno;
static struct class *switch_class;
static struct cdev switch_cdev;


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
    char kbuf[12];
    int len, value;
    value = gpio_get_value(GPIONR);
    len = count < 12 ? count : 12;
    len = snprintf(kbuf, len, "%i", value);
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
    int err=0;
    err = alloc_chrdev_region(&devno, first_minor, max_devices, "switch-driver");
    if(MAJOR(devno) <= 0)
    pr_err("Failed to register chardev\n");
    pr_info("Switch driver got Major %i\n", MAJOR(devno));
    return err;

    switch_class = class_create(THIS_MODULE, "switch-class");
    if (IS_ERR(switch_class)) pr_err("Failed to create class");
    cdev_init(&switch_cdev, &switch_fops);
    err = cdev_add(&switch_cdev, devno, max_devices);
    if (err) pr_err("Failed to add cdev");
 }

 static void mygpio_exit(void)
 {

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