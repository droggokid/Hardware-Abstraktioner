#include <linux/gpio.h> 

 #include <linux/fs.h>

 #include <linux/cdev.h>

 #include <linux/device.h>

 #include <linux/uaccess.h>

 #include <linux/module.h>

 #include <linux/platform_device.h>

 #include <linux/device.h> 

 #include <linux/kdev_t.h>

 #include <linux/of_gpio.h>

 #include <linux/err.h>

const int first_minor = 0;
const int max_devices = 255;
static dev_t devno;
static struct device *gpio_device;
static struct class *gpio_class;
static struct cdev gpio_cdev;

struct gpio_dev {
  int gpio;   // GPIO number
  int flag; // 0: in, 1: out

};

static struct gpio_dev gpio_devs[255];
int gpio_devs_cnt = 0;




static int plat_drv_probe(struct platform_device *pdev){

    int err = 0;
    struct device *dev = &pdev->dev; // Device ptr derived from current platform_device
    struct device_node *np = dev->of_node; // Device tree node ptr
    enum of_gpio_flags flag;
    int gpios_in_dt = 0;

    printk(KERN_DEBUG"New Platform device: %s\n", pdev->name);
    printk ("Hello from probe");

    gpios_in_dt = of_gpio_count(np);

    for(int i = 0; i < gpios_in_dt; i++){
        gpio_devs[i].gpio = of_get_gpio(np, i);
        of_get_gpio_flags(np, i, &flag);
        gpio_devs[i].flag = flag;
    }
    

    for(int i = 0; i < gpios_in_dt; i++){
        //request gpionr
        err = gpio_request(gpio_devs[i].gpio, "plat_drv");

        //set direction for gpio
        if(gpio_devs[i].flag == 1){
            gpio_direction_output(gpio_devs[i].gpio, gpio_devs[i].flag);
        }
        else {
            gpio_direction_input(gpio_devs[i].gpio);
        }
        //create devices
        gpio_device = device_create(gpio_class, NULL, MKDEV(MAJOR(devno), i), NULL, "plat_drv_%d", gpio_devs[i].gpio);
        if(IS_ERR(gpio_device)) pr_err("Failed to create device");
        else(gpio_devs_cnt++);
        
        printk(KERN_ALERT "Using GPIO[%i], flag:%i on major:%i, minor:%i\n",
             gpio_devs[i].gpio, gpio_devs[i].flag,
             MAJOR(devno), i);
    }

    return 0;

}

static int plat_drv_remove(struct platform_device *pdev)

{

    printk(KERN_INFO "Hello from remove\n");

    printk(KERN_ALERT "Removing device %s\n", pdev->name);

    for (int i = 0; i < gpio_devs_cnt; i++) {

            device_destroy(gpio_class, MKDEV(MAJOR(devno), i));
            gpio_free(gpio_devs[i].gpio); // Free GPIOs if necessary

    }

    

    return 0;

}

static const struct of_device_id plat_drv_platform_device_match[] =

{

    { .compatible = "ase, plat_drv",}, {},

};

static struct platform_driver plat_drv_platform_driver = {

    .probe = plat_drv_probe,

    .remove = plat_drv_remove,

    .driver = {

        .name = "plat_drv",

        .of_match_table = plat_drv_platform_device_match,

        .owner = THIS_MODULE, }

 };

int plat_drv_open(struct inode* inode, struct file* filep){

    int major, minor;

    major = MAJOR(inode->i_rdev);

    minor = MINOR(inode->i_rdev);

    printk("Opening MyGpio Device [major], [minor]: %i, %i\n", major, minor);

    return 0;

}

int plat_drv_release(struct inode* inode, struct file* filep){

    int minor, major;

    major = MAJOR(inode->i_rdev);

    minor = MINOR(inode->i_rdev);

    printk("Closing/Releasing MyGpio Device [major], [minor]: %i, %i\n", major, minor);

    return 0;

}

ssize_t plat_drv_read(struct file* filep, char __user *buf, size_t count, loff_t* f_pos){

    char kbuf[12];

    int len, value, minor;

    minor = iminor(filep->f_inode);

    value = gpio_get_value(gpio_devs[minor].gpio);

    len = count < 12 ? count : 12;

    len = snprintf(kbuf, len, "%i", value);

    unsigned long err = copy_to_user(buf, kbuf, ++len);

    if(err) return -1;

    *f_pos += len;

    return len;

}

ssize_t plat_drv_write(struct file *filep, const char __user *ubuf, size_t count, loff_t *f_pos){

    int len, value, err = 0;
    

    char kbuf[12];

    len = count < 12 ? count : 12;

    err = copy_from_user(kbuf, ubuf, len); 

    if(err) return -EFAULT;

    kbuf[len] = 0;

    err = kstrtoint(kbuf, 0, &value);

    if(err) return -EFAULT;

    int minor = iminor(filep->f_inode);
    gpio_set_value(gpio_devs[minor].gpio, value);

    *f_pos += len;

    return len;

}

struct file_operations gpio_fops ={

    .owner = THIS_MODULE,

    .open = plat_drv_open,

    .release = plat_drv_release,

    .read = plat_drv_read,

    .write = plat_drv_write

};

static int __init mygpio_init(void)

{

    // Alloker Major/Minor

    int err=0;

    err = alloc_chrdev_region(&devno, first_minor, max_devices, "my_plat_drv");

    if(MAJOR(devno) < 0)

    pr_err("Failed to register chardev\n");

    pr_info("gpio-Driver got Major %i\n", MAJOR(devno));

    // Class Create 

    gpio_class = class_create(THIS_MODULE, "my_plat_class");

    if (IS_ERR(&gpio_class)) pr_err("Failed to create class");

    //register platform driver

    platform_driver_register(&plat_drv_platform_driver);

    // Cdev Init

    cdev_init(&gpio_cdev, &gpio_fops);


    err = cdev_add(&gpio_cdev, devno, 255);

    if (err) pr_err("Failed to add cdev");

    return err;

}

 static void __exit mygpio_exit(void)

 {

    platform_driver_unregister(&plat_drv_platform_driver);

    // Delete Cdev

    cdev_del(&gpio_cdev);

    // Unregister Device

    unregister_chrdev_region(devno, max_devices);

    // Class Destroy

    class_destroy(gpio_class);

 }

module_init(mygpio_init);

module_exit(mygpio_exit);

MODULE_AUTHOR("Karl");

MODULE_LICENSE("GPL");
