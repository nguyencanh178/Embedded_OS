MQ2SENSOR_VERSION = 1.0
MQ2SENSOR_SITE = $(TOPDIR)/package/mq2sensor
MQ2SENSOR_SITE_METHOD = local

define MQ2SENSOR_INSTALL_TARGET_CMDS
        $(INSTALL) -D -m 0755 $(@D)/mq2sensor.ko $(TARGET_DIR)/root/mq2sensor.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))
