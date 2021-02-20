#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Igor Babin igor.babin@gmail.com");
MODULE_DESCRIPTION("Interrupt for GPIO");

// import exported symbol from gpio_lkm
extern int perform_gpio_command(unsigned int gpio, int command);
extern int read_gpio_value(unsigned int gpio);
extern int gpio_lkm_free_gpio(unsigned int gpio);

// GPIO number for LED and button
static unsigned int gpio_led = 4;
static unsigned int gpio_button = 17;

// buffer for name
static char group_name[10] = "buttonXXX";

// counter for button press number
#define MAX_AMOUNT_PRESS 10
static int counter = 0;


// function for read value (counter)
static ssize_t show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", counter);
}

// function for write value (counter)
static ssize_t store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int p_count;
    sscanf(buf, "%d\n", &p_count);
    counter = p_count;
    return p_count;
}

// structures to create in /sys
#define MODULE_NAME "button_counter"
static struct kobj_attribute counter_attr = __ATTR(counter, 0660, show, store);
static struct attribute *attrs[] = {&counter_attr.attr, NULL};
static struct attribute_group attr_group = {.name = group_name, .attrs = attrs};
static struct kobject *kobj;

// for debounce logic
#define TIME_TO_DEBOUNCE_NANOSEC 600 * 1e6
#define TIME_TO_DEBOUNCE 500000
uint64_t last_press = 0;
static uint64_t debounce_time = (uint64_t)(TIME_TO_DEBOUNCE_NANOSEC);

static irqreturn_t interrupt_handler(int irq, void *dev_id) {
    // get time
    uint64_t current_press = ktime_to_ns(ktime_get());

    // check for debounce
    if (current_press - last_press > debounce_time) {
        // do not react if already max amount
        if (counter >= MAX_AMOUNT_PRESS) return IRQ_RETVAL(1);

        last_press = current_press;
        ++counter;
        printk("[%s] - button pressed %d times\n", MODULE_NAME, counter);

        // if we have got max amount
        if (counter == MAX_AMOUNT_PRESS) {
            printk("[%s] - LED turn on\n", MODULE_NAME);
            perform_gpio_command(gpio_led, 3); // set LED high
        }
    }
    return IRQ_RETVAL(1);
}

// init module
static int __init init(void) {
    int result = 0, irq_num = -1;

    printk(KERN_INFO"[%s] - init functions called\n", MODULE_NAME);
    sprintf(group_name, "button%d", gpio_button);

    if((result = gpio_lkm_free_gpio(gpio_led)) != 0){
        printk(KERN_INFO"[%s] - failed to free LED\n", MODULE_NAME);
        return result;
    };

    if((result = gpio_lkm_free_gpio(gpio_button)) != 0){
        printk(KERN_INFO"[%s] - failed to free button\n", MODULE_NAME);
        return result;
    };

    if (!(kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj->parent))) {
        printk(KERN_INFO"[%s] - failed to create kobject\n", MODULE_NAME);
        return -ENOMEM;
    }

    if ((result = sysfs_create_group(kobj, &attr_group)) != 0) {
        printk(KERN_INFO"[%s] - failed to create sysfs group\n", MODULE_NAME);
        kobject_put(kobj);
        return result;
    }

    if ((result != gpio_request(gpio_led, "sysfs")) != 0) {
        printk(KERN_INFO"[%s] - failed to request GPIO for LED\n", MODULE_NAME);
        return result;
    }

    if ((result = gpio_request(gpio_button, "sysfs")) != 0) {
        printk(KERN_INFO"[%s] - failed to request GPIO for button\n", MODULE_NAME);
        return result;
    }

    if ((result = gpio_direction_output(gpio_led, 0)) != 0) {
        printk(KERN_INFO"[%s] - failed to set GPIO mode for LED\n", MODULE_NAME);
        return result;
    }
    if ((result = gpio_direction_input(gpio_button)) != 0) {
        printk(KERN_INFO"[%s] - failed to set GPIO mode for button\n", MODULE_NAME);
        return result;
    }

    gpio_set_debounce(gpio_button, TIME_TO_DEBOUNCE);

    gpio_export(gpio_led, false);
    gpio_export(gpio_button, false);

    if ((irq_num = gpio_to_irq(gpio_button)) < 0) {
        printk(KERN_INFO"[%s] - failed to get interrupt number for button\n", MODULE_NAME);
        return result;
    } else if((request_irq(irq_num, interrupt_handler, IRQF_TRIGGER_RISING, MODULE_NAME, NULL)) != 0){
        printk(KERN_INFO"[%s] - failed to request interrupt", MODULE_NAME);
        return result;
    }

    printk(KERN_INFO"[%s] - finish init module\n", MODULE_NAME);

    return result;
}

static void __exit deallocate(void) {
    int result;

    kobject_put(kobj);

    if ((result = gpio_to_irq(gpio_button)) > 0) {
        free_irq(result, MODULE_NAME);
    }
    gpio_lkm_free_gpio(gpio_led);
    gpio_lkm_free_gpio(gpio_button);

    printk(KERN_INFO"[%s] - module exit\n", MODULE_NAME);
}

module_init(init);
module_exit(deallocate);

// exported symbols
extern int get_count(void) {
    return counter;
}

EXPORT_SYMBOL(get_count);