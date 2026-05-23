#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irqflags.h>

#define DEVICE_NAME "dht11"
#define CLASS_NAME "dht11_class"

#define GPIO1_BASE_ADDR  0x4804C000
#define GPIO_SIZE        0x1000
#define GPIO_OE          0x134
#define GPIO_DATAIN      0x138
#define GPIO_DATAOUT     0x13C

// P8_15 tướng ứng với GPIO1_15 (GPIO 47)
#define DHT11_PIN     15
#define DHT11_MASK    (1 << DHT11_PIN)

static void __iomem *gpio_base;
static dev_t dev_num;
static struct class *dht_class;
static struct cdev dht_cdev;
static int dht_enabled = 0;

static void dht_set_output(void) {
    u32 oe = ioread32(gpio_base + GPIO_OE);
    oe &= ~DHT11_MASK;
    iowrite32(oe, gpio_base + GPIO_OE);
}

static void dht_set_input(void) {
    u32 oe = ioread32(gpio_base + GPIO_OE);
    oe |= DHT11_MASK;
    iowrite32(oe, gpio_base + GPIO_OE);
}

static void dht_write(int val) {
    u32 out = ioread32(gpio_base + GPIO_DATAOUT);
    if (val) out |= DHT11_MASK;
    else     out &= ~DHT11_MASK;
    iowrite32(out, gpio_base + GPIO_DATAOUT);
}

static int dht_read(void) {
    return (ioread32(gpio_base + GPIO_DATAIN) & DHT11_MASK) ? 1 : 0;
}

static int dht11_read_data(uint8_t *humidity, uint8_t *temperature) {
    uint8_t data[5] = {0};
    int i, j, timeout;
    unsigned long flags;

    // Gửi tín hiệu Start
    dht_set_output();
    dht_write(0);
    mdelay(20); // Đợi 20ms
    dht_write(1);
    udelay(30);
    dht_set_input();

    // Chặn ngắt để đọc dữ liệu chính xác theo micro giây
    local_irq_save(flags);

    // 1. Đợi cảm biến kéo xuống LOW (80us)
    timeout = 1000;
    while (dht_read() && timeout--) udelay(1);
    if (timeout <= 0) { local_irq_restore(flags); return -1; }

    // 2. Đợi cảm biến kéo lên HIGH (80us)
    timeout = 1000;
    while (!dht_read() && timeout--) udelay(1);
    if (timeout <= 0) { local_irq_restore(flags); return -1; }

    // 3. Đợi cảm biến kéo xuống LOW để bắt đầu gửi bit
    timeout = 1000;
    while (dht_read() && timeout--) udelay(1);
    if (timeout <= 0) { local_irq_restore(flags); return -1; }

    // 4. Đọc 40 bits (5 bytes)
    for (j = 0; j < 5; j++) {
        for (i = 0; i < 8; i++) {
            // Đợi mức HIGH (bắt đầu bit)
            timeout = 1000;
            while (!dht_read() && timeout--) udelay(1);
            if (timeout <= 0) { local_irq_restore(flags); return -1; }

            udelay(35); // Đợi qua ngưỡng bit 0 (28us)

            if (dht_read()) {
                data[j] |= (1 << (7 - i));
                // Nếu là bit 1, đợi nó xuống mức LOW lại
                timeout = 1000;
                while (dht_read() && timeout--) udelay(1);
            }
        }
    }

    local_irq_restore(flags); // Khôi phục ngắt

    // Kiểm tra Checksum
    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4])
        return -2;

    *humidity = data[0];
    *temperature = data[2];
    return 0;
}

static ssize_t dht_read_file(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    char result[64];
    int ret;
    uint8_t hum = 0, temp = 0;

    if (*offset > 0) return 0;

    if (!dht_enabled) {
        snprintf(result, sizeof(result), "Sensor is OFF. Send 'ON' to /dev/dht11\n");
    } else {
        ret = dht11_read_data(&hum, &temp);
        if (ret == -1)
            snprintf(result, sizeof(result), "Loi: Cam bien khong phan hoi!\n");
        else if (ret == -2)
            snprintf(result, sizeof(result), "Loi: Sai Checksum (Nhiễu)!\n");
        else
            snprintf(result, sizeof(result), "Nhiet do: %d C | Do am: %d %%\n", temp, hum);
    }

    if (copy_to_user(buf, result, strlen(result)))
        return -EFAULT;

    *offset += strlen(result);
    return strlen(result);
}

static ssize_t dht_write_file(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char command[8] = {0};

    if (copy_from_user(command, buf, min(len, sizeof(command) - 1)))
        return -EFAULT;

    if (strncmp(command, "ON", 2) == 0) {
        dht_enabled = 1;
        pr_info("DHT11: Enabled\n");
    } else if (strncmp(command, "OFF", 3) == 0) {
        dht_enabled = 0;
        pr_info("DHT11: Disabled\n");
    }

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dht_read_file,
    .write = dht_write_file,
};

static int __init dht_init(void) {
    int ret;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;

    cdev_init(&dht_cdev, &fops);
    ret = cdev_add(&dht_cdev, dev_num, 1);
    if (ret < 0) goto unreg_chrdev;

    dht_class = class_create(CLASS_NAME);
    if (IS_ERR(dht_class)) {
        ret = PTR_ERR(dht_class);
        goto cdev_del;
    }

    device_create(dht_class, NULL, dev_num, NULL, DEVICE_NAME);

    gpio_base = ioremap(GPIO1_BASE_ADDR, GPIO_SIZE);
    if (!gpio_base) {
        ret = -ENOMEM;
        goto dev_destroy;
    }

    dht_set_input();
    pr_info("DHT11: Driver loaded. Major=%d\n", MAJOR(dev_num));
    return 0;

dev_destroy:
    device_destroy(dht_class, dev_num);
    class_destroy(dht_class);
cdev_del:
    cdev_del(&dht_cdev);
unreg_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit dht_exit(void) {
    iounmap(gpio_base);
    device_destroy(dht_class, dev_num);
    class_destroy(dht_class);
    cdev_del(&dht_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("DHT11: Driver unloaded\n");
}

module_init(dht_init);
module_exit(dht_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jap");
MODULE_DESCRIPTION("Safe DHT11 Driver for BeagleBone Black (P8_15)");
