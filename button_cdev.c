#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Igor Babin igor.babin185@gmail.com");
MODULE_DESCRIPTION("Cdev for viewing amount of button pressed");

// import exported symbol from button_counter
extern int get_count(void);

// name which will be displayed in /dev
#define DEVICE_NAME "button_counter"

// start of minor numbers requested
#define CHARDEV_MINOR 19

// how many minors requested
#define CHARDEV_MINOR_NUM 1

// structures for character device
static dev_t first;
static struct cdev c_dev;
static struct class *cd_class;

// implement functions to work with character device as with file
static int open(struct inode *i, struct file *f){
    printk(KERN_INFO"[%s] - open() method called\n", DEVICE_NAME);
    return 0;
}

// close function
static int release(struct inode *i, struct file *f){
    printk(KERN_INFO"[%s] - close() method called\n", DEVICE_NAME);
    return 0;
}

// Buffer for reading device
#define size 20
static char message[size] = "";

static ssize_t read(struct file *f, char __user *buf, size_t len, loff_t *off){
    sprintf(message, "%d", get_count());

    if (copy_to_user(buf, message, size))
        return -EFAULT;

    printk(KERN_INFO"[%s] - read() method called, amount of count: %d\n", DEVICE_NAME, get_count());

    return sizeof(int);
}

static ssize_t write(struct file *f, const char __user *buf, size_t len, loff_t *off){
    printk(KERN_INFO"[%s] - write() method called\n", DEVICE_NAME);
    return len;
}


static struct file_operations chardev_fops =
        {
                .owner = THIS_MODULE,
                .open = open,
                .release = release,
                .read = read,
                .write = write
        };

// init device
static int __init init(void)
{
    int result = 0;
    struct device *dev_ret;

    printk(KERN_INFO"[%s] - init functions called", DEVICE_NAME);
    // allocate minor numbers
    if ((result = alloc_chrdev_region(&first, CHARDEV_MINOR, CHARDEV_MINOR_NUM, DEVICE_NAME)) < 0)
    {
        printk(KERN_INFO"[%s] - failed to alloc chrdev", DEVICE_NAME);
        return result;
    }
    // create class for device
    if (IS_ERR(cd_class = class_create(THIS_MODULE, DEVICE_NAME)))
    {
        printk(KERN_INFO"[%s] - failed to create class", DEVICE_NAME);
        unregister_chrdev_region(first, 1);
        return PTR_ERR(cd_class);
    }

    // create device
    if (IS_ERR(dev_ret = device_create(cd_class, NULL, first, NULL, DEVICE_NAME)))
    {
        printk(KERN_INFO"[%s] - failed to create chrdev", DEVICE_NAME);
        class_destroy(cd_class);
        unregister_chrdev_region(first, 1);
        return PTR_ERR(dev_ret);
    }

    // init chrdev structure
    cdev_init(&c_dev, &chardev_fops);
    if ((result = cdev_add(&c_dev, first, 1)) < 0)
    {
        printk(KERN_INFO"[%s] - failed to add chrdev", DEVICE_NAME);
        device_destroy(cd_class, first);
        class_destroy(cd_class);
        unregister_chrdev_region(first, 1);
        return result;
    }
    return result;
}

// deallocate device
static void __exit deallocate(void){
    cdev_del(&c_dev);
    device_destroy(cd_class, first);
    class_destroy(cd_class);
    unregister_chrdev_region(first, 1);
    printk(KERN_INFO "[%s] - unregistered from kernel", DEVICE_NAME);
}

module_init(init);
module_exit(deallocate);