################################################################################
#
# wmt-init
#
################################################################################

WMT_INIT_SITE = $(BR2_EXTERNAL_ABS_CLOCK_PATH)/package/wmt-init/src
WMT_INIT_SITE_METHOD = local
WMT_INIT_LICENSE = GPL-2.0

define WMT_INIT_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -o $(@D)/wmt-init $(@D)/wmt-init.c
endef

define WMT_INIT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/wmt-init $(TARGET_DIR)/usr/sbin/wmt-init
endef

$(eval $(generic-package))
