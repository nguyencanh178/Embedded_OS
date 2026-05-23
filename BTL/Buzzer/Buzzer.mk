BUZZER_VERSION = 1.0
BUZZER_SITE = $(TOPDIR)/package/buzzer
BUZZER_SITE_METHOD = local

# Quan trọng: Phải có ARCH và CROSS_COMPILE
define BUZZER_BUILD_CMDS
        $(MAKE) -C $(LINUX_DIR) \
                ARCH=arm \
                CROSS_COMPILE=$(TARGET_CROSS) \
                M=$(@D) modules
endef

define BUZZER_INSTALL_TARGET_CMDS
        $(MAKE) -C $(LINUX_DIR) \
                ARCH=arm \
                CROSS_COMPILE=$(TARGET_CROSS) \
                M=$(@D) \
                INSTALL_MOD_PATH=$(TARGET_DIR) modules_install
        cp $(@D)/buzzer.ko $(TARGET_DIR)/root/
endef

$(eval $(kernel-module))
$(eval $(generic-package))
