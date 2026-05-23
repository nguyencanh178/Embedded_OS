dht11.mk


DHT11_VERSION = 1.0
DHT11_SITE = $(TOPDIR)/package/dht11
DHT11_SITE_METHOD = local

define DHT11_BUILD_CMDS
        $(MAKE) ARCH=$(KERNEL_ARCH) CROSS_COMPILE="$(TARGET_CROSS)" \
                -C $(LINUX_DIR) M=$(@D) modules
endef

define DHT11_INSTALL_TARGET_CMDS
        $(MAKE) ARCH=$(KERNEL_ARCH) CROSS_COMPILE="$(TARGET_CROSS)" \
                -C $(LINUX_DIR) M=$(@D) INSTALL_MOD_PATH=$(TARGET_DIR) modules_install
        cp $(@D)/dht11.ko $(TARGET_DIR)/root/
endef

$(eval $(kernel-module))
$(eval $(generic-package))





