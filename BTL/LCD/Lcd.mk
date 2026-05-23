LCD2004_VERSION = 1.0
LCD2004_SITE = $(TOPDIR)/package/lcd2004
LCD2004_SITE_METHOD = local

define LCD2004_BUILD_CMDS
        $(MAKE) -C $(LINUX_DIR) \
                ARCH=arm \
                CROSS_COMPILE=$(TARGET_CROSS) \
                M=$(@D) modules
endef

define LCD2004_INSTALL_TARGET_CMDS
        $(MAKE) -C $(LINUX_DIR) \
                ARCH=arm \
                CROSS_COMPILE=$(TARGET_CROSS) \
                M=$(@D) \
                INSTALL_MOD_PATH=$(TARGET_DIR) modules_install

        cp $(@D)/lcd2004.ko $(TARGET_DIR)/root/
endef

$(eval $(kernel-module))
$(eval $(generic-package))
