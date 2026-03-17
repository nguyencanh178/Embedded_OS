Báo cáo Hệ điều hành nhúng - Tuần 6
===================================

Điều khiển LED trên BeagleBone Black và tích hợp ứng dụng vào Buildroot
-----------------------------------------------------------------------

# 1. Mục tiêu

Trong tuần 6, các mục tiêu cần đạt gồm:

-   Điều khiển LED USR3 trên BBB thông qua giao diện **sysfs của Linux**.

-   Viết chương trình `C` để điều khiển LED.

-   Đóng gói chương trình thành `package trong Buildroot`.

-   Tạo chương trình LED nhấp nháy.

-   Tạo dịch vụ tự động chạy khi hệ thống khởi động.

* * * * *

# 2. Điều khiển LED bằng sysfs trên BBB

Trước tiên đăng nhập vào hệ thống BBB thông qua **Serial Terminal**.

### Kiểm tra các LED trên hệ thống

```bash
ls /sys/class/leds/
```

Kết quả hiển thị:

```
beaglebone:green:usr0
beaglebone:green:usr1
beaglebone:green:usr2
beaglebone:green:usr3
```

### Tắt trigger mặc định của LED

LED trên BBB thường được kernel điều khiển bằng trigger mặc định, vì vậy cần tắt trigger để điều khiển thủ công.

```bash
echo none > /sys/class/leds/beaglebone:green:usr3/trigger
```

### Bật LED

```bash
echo 1 > /sys/class/leds/beaglebone:green:usr3/brightness
```

LED **USR3** sẽ sáng.

### Tắt LED

```bash
echo 0 > /sys/class/leds/beaglebone:green:usr3/brightness
```

LED **USR3** sẽ tắt.

### Kiểm tra trạng thái LED

```bash
cat /sys/class/leds/beaglebone:green:usr3/brightness
```

Kết quả:

-   **1** → LED đang bật
-   **0** → LED đang tắt

# 3. Viết chương trình C điều khiển LED


Sau khi điều khiển LED bằng lệnh thành công, tiến hành viết chương trình C để bật tắt LED.

### Tạo file chương trình

```
nano led_control.c
```

### Nội dung chương trình

```c
#include <stdio.h>
#include <unistd.h>

int main() {

    FILE *f = fopen("/sys/class/leds/beaglebone:green:usr3/brightness", "w");
    fprintf(f, "1");
    fclose(f);

    printf("LED USR3 is ON\n");

    sleep(3);

    f = fopen("/sys/class/leds/beaglebone:green:usr3/brightness", "w");
    fprintf(f, "0");
    fclose(f);

    printf("LED USR3 is OFF\n");

    return 0;
}
```

### Nguyên lý hoạt động

Chương trình thực hiện:

-   Mở file ./brightness

-   Ghi giá trị **1** để bật LED

-   Chờ 3s

-   Ghi giá trị **0** để tắt LED

### Chạy chương trình

Sau khi copy chương trình vào board:

```bash
/root/led_control
```

LED sẽ bật trong 3s rồi tắt.

# 4. Tích hợp chương trình vào Buildroot

Sau khi chương trình hoạt động thành công, tiến hành tích hợp ứng dụng vào **Buildroot**.

### Tạo package

```bash
cd ~/workspace/buildroot/package
mkdir led_control
```

Tạo các file cấu hình:

```
touch led_control/Config.in
touch led_control/led_control.mk

```

### File Config.in

```bash
config BR2_PACKAGE_LED_CONTROL
    bool "led_control"
    help
      Day la ung dung dieu khien LED USR3 cho mach BeagleBone Black.
```

### File led_control.mk

```bash
LED_CONTROL_VERSION = 1.0
LED_CONTROL_SITE = $(HOME)
LED_CONTROL_SITE_METHOD = local

define LED_CONTROL_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(@D)/led_control.c -o $(@D)/led_control $(TARGET_LDFLAGS)
endef

define LED_CONTROL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/led_control $(TARGET_DIR)/usr/bin/led_control
endef

$(eval $(generic-package))
```

### Đăng ký package

Mở file:

```bash
nano ~/workspace/buildroot/package/Config.in
```

Thêm dòng:

```bash
source "package/led_control/Config.in"
```

### Build hệ thống

```bash
cd ~/workspace/buildroot
make menuconfig
```

Chọn package **led_control**, sau đó build:

```bash
make
```

# 5. Tạo chương trình LED nhấp nháy

Tiếp theo tạo chương trình để LED nhấp nháy liên tục.

### Tạo chương trình

```bash
nano led_blink.c
```

### Nội dung chương trình

```c
#include <stdio.h>
#include <unistd.h>

#define LED_PATH "/sys/class/leds/beaglebone:green:usr3/brightness"
#define TRIG_PATH "/sys/class/leds/beaglebone:green:usr3/trigger"

void write_file(const char* path, const char* val) {
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", val);
        fclose(f);
    }
}

int main() {

    printf("BBB Blink LED USR3 Starting...\n");

    write_file(TRIG_PATH, "none");

    while(1) {

        write_file(LED_PATH, "1");
        usleep(500000);

        write_file(LED_PATH, "0");
        usleep(500000);
    }

    return 0;
}

```

Chương trình thực hiện:

-   Bật LED

-   Tắt LED

-   Lặp lại liên tục với chu kỳ **0.5 giây**

# 6. Tạo package cho led_blink

### Tạo thư mục package

```bash
cd ~/workspace/buildroot/package
mkdir led_blink

```

### File Config.in

```bash
config BR2_PACKAGE_LED_BLINK
    bool "led_blink"
    help
      Chuong trinh lam nhap nhay LED USR3 cho BeagleBone Black.
```

### File led_blink.mk

```bash
LED_BLINK_VERSION = 1.0
LED_BLINK_SITE = $(HOME)/my_apps
LED_BLINK_SITE_METHOD = local

define LED_BLINK_BUILD_CMDS
    $(TARGET_CC) $(TARGET_CFLAGS) $(@D)/led_blink.c -o $(@D)/led_blink $(TARGET_LDFLAGS)
endef

define LED_BLINK_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/led_blink $(TARGET_DIR)/usr/bin/led_blink
endef

$(eval $(generic-package))

```

### Đăng ký package

Thêm vào:

```bash
source "package/led_control/Config.in"
source "package/led_blink/Config.in"

```

Build hệ thống:

```bash
make menuconfig
make
```

# 7. Chạy chương trình trên BBB

Sau khi nạp image và khởi động BBB:

Kiểm tra chương trình:

```bash
ls /usr/bin
```

Chạy chương trình:

```
led_blink
```

LED **USR3** sẽ nhấp nháy liên tục.

# 8. Tạo script tự chạy khi boot

Tạo file:

```bash
nano S99blink
```

Nội dung:

```bash
#!/bin/sh

case "$1" in
start)
    /usr/bin/led_blink &
    ;;
stop)
    killall led_blink
    ;;
restart)
    $0 stop
    $0 start
    ;;
esac
exit 0
```

Script này cho phép:

-   chạy chương trình khi boot

-   start / stop / restart dịch vụ

* * * * *

# 9. Cập nhật file led_blink.mk

```bash
define LED_BLINK_INSTALL_INIT_SYSV
    $(INSTALL) -D -m 0755 $(HOME)/my_apps/S99blink $(TARGET_DIR)/etc/init.d/S99blink
endef
```

Sau đó build lại hệ thống:

```bash
make
```

* * * * *

# 10. Kết quả đạt được

Sau khi hoàn thành bài thực hành:

-   LED được điều khiển thông qua **sysfs**.

-   Viết được **chương trình C điều khiển LED**.

-   Tích hợp chương trình thành **package trong Buildroot**.

-   Tạo được chương trình **LED nhấp nháy**.

-   Ứng dụng **tự động chạy khi hệ thống khởi động**.