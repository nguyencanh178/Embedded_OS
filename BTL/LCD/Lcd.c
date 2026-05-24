#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>

#define DEVICE_NAME "lcd2004"
#define CLASS_NAME  "lcd2004_class"

#define LCD_I2C_BUS   2
#define LCD_I2C_ADDR  0x27

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RW        0x02
#define LCD_RS        0x01

#define LCD_COLS 20
#define LCD_ROWS 4

static dev_t dev_num;
static struct class *lcd_class;
static struct cdev lcd_cdev;
static struct i2c_client *lcd_client;

static int lcd_i2c_write(unsigned char data)
{
    if (!lcd_client)
        return -ENODEV;

    return i2c_smbus_write_byte(lcd_client, data | LCD_BACKLIGHT);
}

static void lcd_pulse_enable(unsigned char data)
{
    lcd_i2c_write(data | LCD_ENABLE);
    udelay(1);
    lcd_i2c_write(data & ~LCD_ENABLE);
    udelay(50);
}

static void lcd_write4bits(unsigned char data)
{
    lcd_i2c_write(data);
    lcd_pulse_enable(data);
}

static void lcd_send(unsigned char value, unsigned char mode)
{
    unsigned char high;
    unsigned char low;

    high = value & 0xF0;
    low = (value << 4) & 0xF0;

    lcd_write4bits(high | mode);
    lcd_write4bits(low | mode);
}

static void lcd_command(unsigned char cmd)
{
    lcd_send(cmd, 0);

    if (cmd == 0x01 || cmd == 0x02)
        msleep(2);
}

static void lcd_data(unsigned char data)
{
    lcd_send(data, LCD_RS);
}

static void lcd_clear(void)
{
    lcd_command(0x01);
    msleep(2);
}

static void lcd_set_cursor(int row, int col)
{
    static const unsigned char row_offsets[] = {0x00, 0x40, 0x14, 0x54};

    if (row < 0)
        row = 0;
    if (row >= LCD_ROWS)
        row = LCD_ROWS - 1;

    if (col < 0)
        col = 0;
    if (col >= LCD_COLS)
        col = LCD_COLS - 1;

    lcd_command(0x80 | (row_offsets[row] + col));
}

static void lcd_print_line(int row, const char *text)
{
    int i;

    lcd_set_cursor(row, 0);

    for (i = 0; i < LCD_COLS; i++) {
        if (text && text[i] != '\0')
            lcd_data(text[i]);
        else
            lcd_data(' ');
    }
}

static void lcd_init_hw(void)
{
    msleep(50);

    lcd_write4bits(0x30);
    msleep(5);

    lcd_write4bits(0x30);
    udelay(150);

    lcd_write4bits(0x30);
    udelay(150);

    lcd_write4bits(0x20);
    udelay(150);

    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x06);
    lcd_clear();

    lcd_print_line(0, "LCD2004 READY");
    lcd_print_line(1, "Gas Monitor");
    lcd_print_line(2, "BBB Buildroot");
    lcd_print_line(3, "Waiting data...");
}

static void lcd_show_text(const char *buf)
{
    char lines[LCD_ROWS][LCD_COLS + 1];
    int r;
    int c;
    int i;

    for (r = 0; r < LCD_ROWS; r++) {
        for (c = 0; c < LCD_COLS; c++)
            lines[r][c] = ' ';
        lines[r][LCD_COLS] = '\0';
    }

    r = 0;
    c = 0;

    for (i = 0; buf[i] != '\0' && r < LCD_ROWS; i++) {
        if (buf[i] == '\n' || buf[i] == '|') {
            r++;
            c = 0;
            continue;
        }

        if (c < LCD_COLS) {
            lines[r][c] = buf[i];
            c++;
        }
    }

    for (r = 0; r < LCD_ROWS; r++)
        lcd_print_line(r, lines[r]);
}

static ssize_t lcd_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    char *kbuf;
    size_t copy_len;

    if (len == 0)
        return 0;

    copy_len = min(len, (size_t)255);

    kbuf = kzalloc(copy_len + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, copy_len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[copy_len] = '\0';

    if (strncmp(kbuf, "CLEAR", 5) == 0)
        lcd_clear();
    else
        lcd_show_text(kbuf);

    kfree(kbuf);

    return len;
}

static struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
};

static int __init lcd_init(void)
{
    int ret;
    struct i2c_adapter *adapter;
    struct i2c_board_info info = {
        I2C_BOARD_INFO("lcd2004_pcf8574", LCD_I2C_ADDR)
    };

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&lcd_cdev, &lcd_fops);

    ret = cdev_add(&lcd_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    lcd_class = class_create(CLASS_NAME);
    if (IS_ERR(lcd_class)) {
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(lcd_class);
    }

    if (IS_ERR(device_create(lcd_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        class_destroy(lcd_class);
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    adapter = i2c_get_adapter(LCD_I2C_BUS);
    if (!adapter) {
        pr_err("LCD2004: cannot get i2c adapter %d\n", LCD_I2C_BUS);
        device_destroy(lcd_class, dev_num);
        class_destroy(lcd_class);
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -ENODEV;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    lcd_client = i2c_new_client_device(adapter, &info);
#else
    lcd_client = i2c_new_device(adapter, &info);
#endif

    i2c_put_adapter(adapter);

    if (IS_ERR_OR_NULL(lcd_client)) {
        pr_err("LCD2004: cannot create i2c client at 0x%x\n", LCD_I2C_ADDR);
        device_destroy(lcd_class, dev_num);
        class_destroy(lcd_class);
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -ENODEV;
    }

    lcd_init_hw();

    pr_info("LCD2004 driver loaded, i2c-%d addr=0x%x, major=%d\n",
            LCD_I2C_BUS, LCD_I2C_ADDR, MAJOR(dev_num));

    return 0;
}

static void __exit lcd_exit(void)
{
    if (lcd_client) {
        lcd_clear();
        lcd_print_line(0, "LCD2004 OFF");
        i2c_unregister_device(lcd_client);
    }

    device_destroy(lcd_class, dev_num);
    class_destroy(lcd_class);
    cdev_del(&lcd_cdev);
    unregister_chrdev_region(dev_num, 1);

    pr_info("LCD2004 driver unloaded\n");
}

module_init(lcd_init);
module_exit(lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YourName");
MODULE_DESCRIPTION("LCD 2004 I2C PCF8574 driver for BeagleBone Black");
