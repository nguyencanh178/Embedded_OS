APPSENSOR_VERSION = 1.0
APPSENSOR_SITE = $(TOPDIR)/package/appsensor
APPSENSOR_SITE_METHOD = local
APPSENSOR_DEPENDENCIES = cjson

define APPSENSOR_BUILD_CMDS
        $(TARGET_CC) $(TARGET_CFLAGS) -o $(@D)/appsensor $(APPSENSOR_SITE)/app.c $(TARGET_LDFLAGS) -lcjson
endef

define APPSENSOR_INSTALL_TARGET_CMDS
        # Copy file thuc thi vao /usr/bin
        $(INSTALL) -D -m 0755 $(@D)/appsensor $(TARGET_DIR)/usr/bin/appsensor
        # Copy file tu dong khoi dong vao thu muc he thong
        $(INSTALL) -D -m 0755 $(APPSENSOR_SITE)/S99appsensor $(TARGET_DIR)/etc/init.d/S99appsensor
endef

$(eval $(generic-package))
