/*
 * enumerate and monitor v4l devices
 *
 * Copyright (C) 2009 Kay Sievers <kay.sievers@vrfy.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE 1
#include "libudev.h"

int main(int argc, char *argv[])
{
	struct udev *udev;
	struct udev_monitor *monitor;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry;

	/* libudev context */
	udev = udev_new();

	/* connect to event source */
	monitor = udev_monitor_new_from_netlink(udev, "udev");
	/* install subsytem filter, we will not wake-up for other events */
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", NULL);
	/* listen to events, and buffer them */
	udev_monitor_enable_receiving(monitor);

	/* prepare a device scan */
	enumerate = udev_enumerate_new(udev);
	/* filter for video devices */
	udev_enumerate_add_match_subsystem(enumerate,"usb");
	/* filter for capture capable devices */
//	udev_enumerate_add_match_property(enumerate, "ID_V4L_CAPABILITIES", "*:capture:*");
	/* retrieve the list */
	udev_enumerate_scan_devices(enumerate);
	/* print devices */
	udev_list_entry_foreach(list_entry, udev_enumerate_get_list_entry(enumerate)) {
		struct udev_device *device;

		device = udev_device_new_from_syspath(udev_enumerate_get_udev(enumerate),
						      udev_list_entry_get_name(list_entry));
		if (device == NULL)
			continue;
		printf("%s: ", udev_list_entry_get_name(list_entry));
		printf("(%s)\n",
		       udev_device_get_devnode(device));
		udev_device_unref(device);
	}
	udev_enumerate_unref(enumerate);

	udev_monitor_unref(monitor);

	udev_unref(udev);
	return 0;
}
