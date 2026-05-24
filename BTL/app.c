#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#define MQ2_PATH "/dev/mq2adc"
#define DHT11_PATH "/dev/dht11"
#define BUZZER_PATH "/dev/buzzer"
#define WATCHDOG_PATH "/dev/watchdog"
#define LCD_PATH "/dev/lcd2004"
#define LOG_PATH "/var/log/sensor_monitor.log"

#define GAS_THRESHOLD 30000
#define LOOP_DELAY 2

int watchdog_fd = -1;
int lcd_fd = -1;

void print_console(const char *msg) {
    FILE *con = fopen("/dev/console", "a");

    if (con) {
        fprintf(con, "%s\n", msg);
        fclose(con);
    }
}

void log_text(const char *msg) {
    FILE *log = fopen(LOG_PATH, "a");

    if (log) {
        fprintf(log, "%s\n", msg);
        fclose(log);
    }

    print_console(msg);
}

int read_mq2() {
    int fd = open(MQ2_PATH, O_RDONLY);

    if (fd < 0)
        return -1;

    char buf[64] = {0};
    int val = -1;

    if (read(fd, buf, sizeof(buf) - 1) > 0) {
        sscanf(buf, "ADC: %d", &val);
    }

    close(fd);

    return val;
}

int read_dht11(int *temp, int *hum) {
    int fd = open(DHT11_PATH, O_RDONLY);

    if (fd < 0)
        return -1;

    char buf[128] = {0};

    if (read(fd, buf, sizeof(buf) - 1) > 0) {
        sscanf(buf,
               "Nhiet do: %d C | Do am: %d %%",
               temp,
               hum);

        close(fd);
        return 0;
    }

    close(fd);
    return -1;
}

int control_buzzer(int on) {
    int fd = open(BUZZER_PATH, O_WRONLY);

    if (fd < 0)
        return -1;

    if (on)
        write(fd, "ON", 2);
    else
        write(fd, "OFF", 3);

    close(fd);
    return 0;
}

void update_lcd(int gas, int temp, int hum, int alert) {
    char lcd_buf[128];

    if (lcd_fd < 0) {
        lcd_fd = open(LCD_PATH, O_WRONLY);
    }

    if (lcd_fd < 0) {
        return;
    }

    snprintf(lcd_buf,
             sizeof(lcd_buf),
             "Gas : %-6d ADC|Temp: %-2d C|Hum : %-2d %%|%s",
             gas,
             temp,
             hum,
             alert ? "GAS HIGH! BUZZER ON" : "Status: SAFE");

    write(lcd_fd, lcd_buf, strlen(lcd_buf));
}

int main() {
    int last_buzzer = 0;

    watchdog_fd = open(WATCHDOG_PATH, O_RDWR);
    lcd_fd = open(LCD_PATH, O_WRONLY);

    log_text("");
    log_text("==============================================");
    log_text("      GAS MONITORING SYSTEM STARTED");
    log_text("==============================================");
    log_text("Time       | Gas ADC | Temp | Humidity");
    log_text("----------------------------------------------");

    while (1) {
        int gas = read_mq2();
        int temp = 0;
        int hum = 0;
        char line[128];

        read_dht11(&temp, &hum);

        if (gas < 0) {
            sleep(LOOP_DELAY);
            continue;
        }

        if (gas >= GAS_THRESHOLD) {
            if (!last_buzzer) {
                control_buzzer(1);
                last_buzzer = 1;
                update_lcd(gas, temp, hum, last_buzzer);
                log_text("!!! WARNING: GAS HIGH - BUZZER ON !!!");
            }
        } else {
            if (last_buzzer) {
                control_buzzer(0);
                last_buzzer = 0;
                update_lcd(gas, temp, hum, last_buzzer);
                log_text("Gas normal - Buzzer OFF");
            }
        }

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        snprintf(line,
                 sizeof(line),
                 "%02d:%02d:%02d   | %-7d | %2d C | %2d %%",
                 t->tm_hour,
                 t->tm_min,
                 t->tm_sec,
                 gas,
                 temp,
                 hum);

        log_text(line);

        update_lcd(gas, temp, hum, last_buzzer);

        if (watchdog_fd >= 0) {
            ioctl(watchdog_fd, WDIOC_KEEPALIVE, 0);
        }

        sleep(LOOP_DELAY);
    }

    return 0;
}
