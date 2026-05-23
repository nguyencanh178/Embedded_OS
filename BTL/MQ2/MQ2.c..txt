#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define DEVICE_NAME "mq2adc"
#define CLASS_NAME "mq2adc_class"

#define ADS1115_ADDR 0x48
#define ADS1115_REG_CONVERT  0x00
#define ADS1115_REG_CONFIG   0x01
#define ADS1115_CONFIG_MSB 0xC2
#define ADS1115_CONFIG_LSB 0x83

#define GPIO1_BASE 0x4804C000
#define GPIO_SIZE  0x1000
#define GPIO_OE         0x134
#define GPIO_DATAIN     0x138
#define GPIO_DATAOUT    0x13C
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190

#define SCL_PIN 17  // P9_23
#define SDA_PIN 16  // P9_15
#define SCL_MASK (1 << SCL_PIN)
#define SDA_MASK (1 << SDA_PIN)
#define I2C_DELAY 5

static dev_t dev_num;
static struct class *mq2adc_class = NULL;
static struct cdev mq2adc_cdev;
static void __iomem *gpio_base;
static bool sensor_enabled = false;

static inline void set_scl(int value) {
    if (value) iowrite32(SCL_MASK, gpio_base + GPIO_SETDATAOUT);
    else iowrite32(SCL_MASK, gpio_base + GPIO_CLEARDATAOUT);
    udelay(I2C_DELAY);
}

static inline void set_sda(int value) {
    if (value) iowrite32(SDA_MASK, gpio_base + GPIO_SETDATAOUT);
    else iowrite32(SDA_MASK, gpio_base + GPIO_CLEARDATAOUT);
    udelay(I2C_DELAY);
}

static inline int read_sda(void) {
    u32 val, oe_val;
    oe_val = ioread32(gpio_base + GPIO_OE);
    iowrite32(oe_val | SDA_MASK, gpio_base + GPIO_OE);
    val = ioread32(gpio_base + GPIO_DATAIN);
    iowrite32(oe_val, gpio_base + GPIO_OE);
    return (val & SDA_MASK) ? 1 : 0;
}

static void i2c_start(void) {
    u32 reg_val = ioread32(gpio_base + GPIO_OE);
    iowrite32(reg_val & ~SDA_MASK, gpio_base + GPIO_OE);
    set_sda(1); set_scl(1); set_sda(0); set_scl(0);
}

static void i2c_stop(void) {
    u32 reg_val = ioread32(gpio_base + GPIO_OE);
    iowrite32(reg_val & ~SDA_MASK, gpio_base + GPIO_OE);
    set_sda(0); set_scl(1); set_sda(1);
}

static int i2c_write_byte(unsigned char byte) {
    int i, ack;
    u32 reg_val = ioread32(gpio_base + GPIO_OE);
    iowrite32(reg_val & ~SDA_MASK, gpio_base + GPIO_OE);
    for (i = 7; i >= 0; i--) {
        set_sda((byte >> i) & 1);
        set_scl(1); set_scl(0);
    }
    iowrite32(reg_val | SDA_MASK, gpio_base + GPIO_OE);
    set_scl(1); ack = !read_sda(); set_scl(0);
    return ack;
}

static unsigned char i2c_read_byte(int ack) {
    int i; unsigned char byte = 0;
    u32 reg_val = ioread32(gpio_base + GPIO_OE);
    iowrite32(reg_val | SDA_MASK, gpio_base + GPIO_OE);
    for (i = 7; i >= 0; i--) {
        set_scl(1);
        if (read_sda()) byte |= (1 << i);
        set_scl(0);
    }
    iowrite32(reg_val & ~SDA_MASK, gpio_base + GPIO_OE);
    set_sda(!ack); set_scl(1); set_scl(0);
    return byte;
}

static int ads1115_read_adc(int16_t *adc_value) {
    unsigned char msb, lsb;
    i2c_start();
    if (!i2c_write_byte(ADS1115_ADDR << 1)) goto err;
    i2c_write_byte(ADS1115_REG_CONFIG);
    i2c_write_byte(ADS1115_CONFIG_MSB);
    i2c_write_byte(ADS1115_CONFIG_LSB);
    i2c_stop();
    msleep(20);
    i2c_start();
    if (!i2c_write_byte(ADS1115_ADDR << 1)) goto err;
    i2c_write_byte(ADS1115_REG_CONVERT);
    i2c_stop();
    i2c_start();
    if (!i2c_write_byte((ADS1115_ADDR << 1) | 1)) goto err;
    msb = i2c_read_byte(1); lsb = i2c_read_byte(0);
    i2c_stop();
    *adc_value = (msb << 8) | lsb;
    return 0;
err:
    i2c_stop(); return -EIO;
}

static ssize_t mq2adc_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    int16_t adc = 0;
    char out[64];
    int len;

    if (*offset > 0) return 0;

    if (!sensor_enabled) {
        len = snprintf(out, sizeof(out), "Sensor OFF\n");
    } else {
        if (ads1115_read_adc(&adc) < 0) {
            len = snprintf(out, sizeof(out), "Loi doc ADC!\n");
        } else {
            // Đảm bảo in ra giá trị ADC thay vì chữ ON
            len = snprintf(out, sizeof(out), "ADC: %d\n", adc);
        }
    }

    if (copy_to_user(buf, out, len)) return -EFAULT;
    *offset = len;
    return len;
}

static ssize_t mq2adc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char cmd[16] = {0};
    if (count > sizeof(cmd) - 1) count = sizeof(cmd) - 1;
    if (copy_from_user(cmd, buf, count)) return -EFAULT;

    // So sánh linh hoạt hơn để tránh lỗi ký tự \n từ lệnh echo
    if (strncmp(cmd, "ON", 2) == 0) {
        sensor_enabled = true;
        pr_info("MQ2: Enabled\n");
    } else if (strncmp(cmd, "OFF", 3) == 0) {
        sensor_enabled = false;
        pr_info("MQ2: Disabled\n");
    }
    return count;
}

static const struct file_operations mq2adc_fops = {
    .owner = THIS_MODULE,
    .read = mq2adc_read,
    .write = mq2adc_write,
};

static int __init mq2adc_init(void) {
    u32 reg_val;
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) return -1;

    cdev_init(&mq2adc_cdev, &mq2adc_fops);
    if (cdev_add(&mq2adc_cdev, dev_num, 1) < 0) goto unreg;

    mq2adc_class = class_create(CLASS_NAME);
    if (IS_ERR(mq2adc_class)) goto cdev_err;

    if (IS_ERR(device_create(mq2adc_class, NULL, dev_num, NULL, DEVICE_NAME))) goto class_err;

    gpio_base = ioremap(GPIO1_BASE, GPIO_SIZE);
    if (!gpio_base) goto dev_err;

    reg_val = ioread32(gpio_base + GPIO_OE);
    reg_val &= ~(SCL_MASK | SDA_MASK);
    iowrite32(reg_val, gpio_base + GPIO_OE);

    pr_info("MQ2 Driver Loaded\n");
    return 0;

dev_err:
    device_destroy(mq2adc_class, dev_num);
class_err:
    class_destroy(mq2adc_class);
cdev_err:
    cdev_del(&mq2adc_cdev);
unreg:
    unregister_chrdev_region(dev_num, 1);
    return -1;
}

static void __exit mq2adc_exit(void) {
    iounmap(gpio_base);
    device_destroy(mq2adc_class, dev_num);
    class_destroy(mq2adc_class);
    cdev_del(&mq2adc_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("MQ2 Driver Unloaded\n");
}

module_init(mq2adc_init);
module_exit(mq2adc_exit);
MODULE_LICENSE("GPL");
