################################################################################
#
# x04g-utils
#
################################################################################

X04G_UTILS_SITE = $(BR2_EXTERNAL_ABS_CLOCK_PATH)/package/x04g-utils/src
X04G_UTILS_SITE_METHOD = local
X04G_UTILS_LICENSE = GPL-2.0

define X04G_UTILS_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -o $(@D)/reboot2 $(@D)/reboot2.c
endef

define X04G_UTILS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/reboot2 $(TARGET_DIR)/usr/sbin/reboot2
endef

$(eval $(generic-package))
