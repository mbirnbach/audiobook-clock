// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal replacement for MediaTek's Android wmt_loader + wmt_launcher for
 * the MT8167 CONNSYS (SoC-integrated Wi-Fi/BT) on mainline Linux.
 *
 * 1. /dev/wmtdetect: query SoC chip id, set it, trigger driver init.
 * 2. /dev/stpwmt: configure BTIF mode + cfg file, then serve "srh_patch"
 *    requests by handing the kernel the ROM patch names.
 *
 * usage: wmt-init [patch-name-prefix]
 * The mainline port loads patches with request_firmware() and wants bare
 * names (no prefix); Android's launcher passes "/vendor/firmware/<name>".
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define WMT_DETECT_IOC_MAGIC		'w'
#define COMBO_IOCTL_SET_CHIP_ID		_IOW(WMT_DETECT_IOC_MAGIC, 1, int)
#define COMBO_IOCTL_GET_SOC_CHIP_ID	_IOR(WMT_DETECT_IOC_MAGIC, 3, int)
#define COMBO_IOCTL_DO_MODULE_INIT	_IOR(WMT_DETECT_IOC_MAGIC, 4, int)

#define WMT_IOC_MAGIC			0xa0
#define WMT_IOCTL_SET_STP_MODE		_IOW(WMT_IOC_MAGIC, 5, int)
#define WMT_IOCTL_GET_CHIP_INFO		_IOR(WMT_IOC_MAGIC, 12, int)
#define WMT_IOCTL_SET_LAUNCHER_KILL	_IOW(WMT_IOC_MAGIC, 13, int)
#define WMT_IOCTL_SET_PATCH_NUM		_IOW(WMT_IOC_MAGIC, 14, int)
#define WMT_IOCTL_SET_PATCH_INFO	_IOW(WMT_IOC_MAGIC, 15, char *)
#define WMT_IOCTL_WMT_CFG_NAME		_IOWR(WMT_IOC_MAGIC, 21, char *)
#define WMT_IOCTL_WMT_TELL_CHIPID	_IOW(WMT_IOC_MAGIC, 23, int)

#define STP_BTIF_FULL			0x03
/* bits 4-7 of the STP mode word: 0 is rejected by the vendor kernel */
#define WMT_FM_COMM			2
#define FW_DIR				"/lib/firmware"
#define PATCH_PREFIX			"ROMv2_lm_patch"
#define CFG_NAME			"WMT_SOC.cfg"

struct wmt_patch_info {
	uint32_t download_seq;
	uint8_t address[4];
	char name[256];
};

static int wmt_fd = -1;
static const char *patch_prefix = "";

static int open_retry(const char *path)
{
	for (int i = 0; i < 50; i++) {
		int fd = open(path, O_RDWR | O_NOCTTY);
		if (fd >= 0)
			return fd;
		usleep(100000);
	}
	fprintf(stderr, "wmt-init: cannot open %s: %s\n", path, strerror(errno));
	return -1;
}

static int detect(void)
{
	int fd = open_retry("/dev/wmtdetect");
	if (fd < 0)
		return -1;

	int chip = ioctl(fd, COMBO_IOCTL_GET_SOC_CHIP_ID, 0);
	printf("wmt-init: soc chip id 0x%04x\n", chip);
	if (chip <= 0) {
		close(fd);
		return -1;
	}
	ioctl(fd, COMBO_IOCTL_SET_CHIP_ID, chip);
	int ret = ioctl(fd, COMBO_IOCTL_DO_MODULE_INIT, chip);
	printf("wmt-init: module init returned %d\n", ret);
	close(fd);
	return chip;
}

static int search_patch(void)
{
	int fwver = ioctl(wmt_fd, WMT_IOCTL_GET_CHIP_INFO, 2);
	int first = 1, found = 0;
	DIR *dir = opendir(FW_DIR);
	struct dirent *de;

	printf("wmt-init: srh_patch, fw version 0x%04x\n", fwver);
	if (!dir)
		return -1;

	while ((de = readdir(dir))) {
		char path[512];
		uint8_t hdr[28];

		if (strncmp(de->d_name, PATCH_PREFIX, strlen(PATCH_PREFIX)))
			continue;
		snprintf(path, sizeof(path), FW_DIR "/%s", de->d_name);
		int pfd = open(path, O_RDONLY);
		if (pfd < 0)
			continue;
		ssize_t n = read(pfd, hdr, sizeof(hdr));
		close(pfd);
		if (n != sizeof(hdr))
			continue;

		unsigned int patchver = (hdr[22] << 8) | hdr[23];
		if ((patchver ^ fwver) & 0xff)
			continue;

		struct wmt_patch_info info = { 0 };
		info.download_seq = hdr[24] & 0x0f;
		memcpy(info.address, &hdr[24], 4);
		info.address[0] = 0;
		snprintf(info.name, sizeof(info.name), "%s%s", patch_prefix, de->d_name);

		if (first) {
			int num = (hdr[24] & 0xf0) >> 4;
			printf("wmt-init: patch count %d\n", num);
			ioctl(wmt_fd, WMT_IOCTL_SET_PATCH_NUM, num);
			first = 0;
		}
		printf("wmt-init: patch seq %u %s\n", info.download_seq, info.name);
		ioctl(wmt_fd, WMT_IOCTL_SET_PATCH_INFO, &info);
		found++;
	}
	closedir(dir);
	return found ? 0 : -1;
}

int main(int argc, char **argv)
{
	if (argc > 1)
		patch_prefix = argv[1];

	int chip = detect();
	if (chip < 0)
		return 1;

	wmt_fd = open_retry("/dev/stpwmt");
	if (wmt_fd < 0)
		return 1;

	char cfg[257] = CFG_NAME;
	ioctl(wmt_fd, WMT_IOCTL_WMT_TELL_CHIPID, chip);
	ioctl(wmt_fd, WMT_IOCTL_SET_STP_MODE, (WMT_FM_COMM << 4) | STP_BTIF_FULL);
	ioctl(wmt_fd, WMT_IOCTL_WMT_CFG_NAME, cfg);
	ioctl(wmt_fd, WMT_IOCTL_SET_LAUNCHER_KILL, 0);
	printf("wmt-init: ready, serving requests\n");
	fflush(stdout);

	for (;;) {
		struct pollfd pfd = { .fd = wmt_fd, .events = POLLIN };
		char cmd[257] = { 0 };

		if (poll(&pfd, 1, -1) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		ssize_t n = read(wmt_fd, cmd, sizeof(cmd) - 1);
		if (n <= 0)
			continue;
		cmd[n] = 0;

		const char *resp = "ok";
		printf("wmt-init: request '%s'\n", cmd);
		if (strstr(cmd, "patch")) {
			if (search_patch())
				resp = "fail";
		}
		write(wmt_fd, resp, strlen(resp));
		fflush(stdout);
	}

	ioctl(wmt_fd, WMT_IOCTL_SET_LAUNCHER_KILL, 1);
	return 0;
}
