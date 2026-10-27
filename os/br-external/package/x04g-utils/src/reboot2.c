// SPDX-License-Identifier: GPL-2.0
/*
 * reboot2 [arg] - reboot with a command string, e.g. "bootloader" or
 * "recovery". The MediaTek watchdog driver passes it to LK, which then
 * stays in fastboot mode (Android's "reboot bootloader").
 */
#include <linux/reboot.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	sync();
	if (argc > 1)
		syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_RESTART2, argv[1]);
	else
		syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_RESTART, NULL);
	perror("reboot");
	return 1;
}
