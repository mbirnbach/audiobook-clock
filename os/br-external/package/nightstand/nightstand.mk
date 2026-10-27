################################################################################
#
# nightstand
#
################################################################################

# The app lives next to the OS tree in this repository
NIGHTSTAND_SITE = $(BR2_EXTERNAL_ABS_CLOCK_PATH)/../../app
NIGHTSTAND_SITE_METHOD = local
NIGHTSTAND_LICENSE = Proprietary
NIGHTSTAND_DEPENDENCIES = qt6base qt6declarative qt6multimedia qt6tools
NIGHTSTAND_RSYNC_EXCLUDES = build

define NIGHTSTAND_INSTALL_AP_HELPER
	$(INSTALL) -D -m 0755 $(@D)/scripts/nightstand-ap \
		$(TARGET_DIR)/usr/libexec/nightstand/nightstand-ap
endef
NIGHTSTAND_POST_INSTALL_TARGET_HOOKS += NIGHTSTAND_INSTALL_AP_HELPER

$(eval $(cmake-package))
