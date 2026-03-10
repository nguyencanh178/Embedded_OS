# TỔNG HỢP HỆ ĐIỀU HÀNH NHÚNG

## 1. U-boot và Kernel

### 1.1. Uboot

#### Biên dịch 
 
- Sử dụng Cross-Compiler Toolchain đã được cấu hình để build uboot

- Đầu ra: `MLO - Secondary Program Loader` chạy trước sau đó `u-boot.img - main bootloader` giao diện dòng lệnh

```
# Clean project
make distclean

# Cấu hình cho BBB
make am335x_evm_defconfig

# Biên dịch
make -j4
```

```
arm-linux-gnueabihf-
```

- Build U-Boot cho BBB 

```
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot
make distclean
make am335x_evm_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

- Tạo thẻ SD cho BBB

| Partition | FS    | Mục đích         |
| --------- | ----- | ---------------- |
| p1        | FAT32 | Boot             |
| p2        | ext4  | rootfs           |

Mount p1:
```
sudo mount /dev/sdX1 /mnt/sd
```

```
cp MLO u-boot.img /mnt/sd
sync
```

### 1.2. Biên dịch & cài Kernel

- Build Linux Kernel cho BBB

```
git clone https://github.com/beagleboard/linux.git
cd linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bb.org_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage dtbs
```
File cần có:

- `arch/arm/boot/zImage`

- `arch/arm/boot/dts/am335x-boneblack.dtb`

- Copy Kernel vào SD

```
cp arch/arm/boot/zImage /mnt/sd
cp arch/arm/boot/dts/am335x-boneblack.dtb /mnt/sd
sync
```

### 1.3. Boot Kernel bằng U-Boot

- Load Kernel & DTB vào RAM

```
load mmc 0:1 0x82000000 zImage
load mmc 0:1 0x88000000 am335x-boneblack.dtb
```

- Boot kernel

```
bootz 0x82000000 - 0x88000000
```

## 2. RootFS

### 2.1. Tải và giải nén BusyBox

```
wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2
tar -xjf busybox-1.36.1.tar.bz2
cd busybox-1.36.1
```
kiểm tra thư mục `ls`


### 2.2. Các bước cấu hình BusyBox
```
make menuconfig
```
Trong menu chọn `Settings  →  Build static binary` chọn `Build BusyBox as a static binary`

Sau khi install → file nằm trong thư mục:
```
_busybox_install/
```

### 2.3. Cấu trúc thư mục tối thiểu của RootFS

```
/bin
/sbin
/etc
/proc
/sys
/dev
/tmp
/usr
```

### 2.4. Biên dịch BusyBox

```
make -j4
```
sau khi build xong sẽ tự tạo file `busybox`

- Cài BusyBox vào thư mục RootFS

```
make install
```
sau đó sẽ có thư mục `_install/` và check `ls _install` sẽ thấy 
```
bin
sbin
usr
linuxrc
```

- Tạo các thư mục cần thiết cho RootFS

Trỏ vào thư mục rootfs `cd _install` và tạo thêm các thư mục hệ thống

```
mkdir dev proc sys etc tmp root var mnt
```

kiểm tra `ls`

- Tạo script khởi động RootFS

Khởi tạo file `init`
```
nano init
```
Copy:
```
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys

echo "RootFS Boot Success"

exec /bin/sh
```

Lưu lại và cấp quyền

```
chmod +x init
```

### 2.5. Chuyển dữ liệu vào thẻ nhớ

- Phân vùng thẻ nhớ , tạo ít nhất một phân vùng định dạng ext4 cho RootFS. Mở công cụ phân vùng

```
sudo cfdisk /dev/sdb
```

Copy RootFS:

```
sudo cp -r * /mnt/
sync
```
Unmount:

```bash
sudo umount /mnt
```

- Liên kết Kernel với RootFS (bootargs)

Vào U-Boot trên board và gõ:
```
setenv bootargs console=ttyS0,115200 root=/dev/mmcblk0p2 rw rootwait
saveenv
boot
```
Giải thích:
```bash
console=ttyS0,115200  → xuất log ra UART
root=/dev/mmcblk0p2   → phân vùng RootFS
rw                    → read write
rootwait              → đợi thẻ nhớ sẵn sàng
```

### 2.6. Kiểm tra sau khi Boot

Nếu thành công sẽ thấy shell:
``
/ #
``
Thử các lệnh:
```bash
ls
cat /proc/cpuinfo
echo hello
```
- Kiểm tra BusyBox có những lệnh gì

```bash
ls /bin
```

## 3. System Build

### 3.1. Build image cơ bản cho BBB và boot thành công

- Chuẩn bị môi trường trên Ubuntu

```bash
sudo apt update
sudo apt install -y git build-essential gcc g++ make \
    ncurses-dev rsync bc bison flex python3 unzip file wget cpio
```
Buildroot yêu cầu chạy trên Linux host và cần các tool build chuẩn như make, gcc, g++, rsync, bc, cpio, file…

- Lấy mã nguồn Buildroot

```bash
git clone https://gitlab.com/buildroot.org/buildroot.git
cd buildroot
```

- Nạp cấu hình mẫu cho BeagleBone

```bash
make beaglebone_defconfig
make menuconfig
```

Trong menuconfig, kiểm tra các mục chính:
```bash
Target options → ARM / Cortex-A8

Toolchain → để Buildroot toolchain

Bootloaders → U-Boot

Kernel → Linux kernel

Filesystem images → chọn ext2/4 hoặc tar tùy mục đích
```
Buildroot có thể build độc lập từng phần: bootloader, kernel, rootfs và toolchain

### 3.2. Dùng toolchain Buildroot để biên dịch Hello World

- Tạo chương trình mẫu

```c
#include <stdio.h>

int main(void) {
    printf("Hello World from Buildroot toolchain on BBB!\n");
    return 0;
}
```

- Dùng toolchain Buildroot để biên dịch trên PC

```bash
export PATH=$PWD/output/host/bin:$PATH
```
kiểm tra tên compiler

```bash
ls output/host/bin/*gcc
```

sau đó biên dịch

```bash
arm-buildroot-linux-gnueabihf-gcc hello.c -o hello
```

- Chép chương trình vào RootFS trên thẻ nhớ sử dụng ghi trực tiếp 

```Bash
sudo mount /dev/sdX2 /mnt
sudo cp hello /mnt/usr/bin/
sync
sudo umount /mnt
```

- Chạy thử chương trình trên BBB

```bash
chmod +x /usr/bin/hello
/usr/bin/hello
```
