/*
 * lsusb
 *
 * Copyright (C) 2009 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2009 Greg Kroah-Hartman <greg@kroah.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <fcntl.h>
#include <poll.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/stat.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE

#include <libudev.h>
#include "list.h"
#include "usb.h"
#include "lsusb.h"



static LIST_HEAD(usb_devices);
struct udev *udev;

void *robust_malloc(size_t size)
{
	void *data;

	data = malloc(size);
	if (data == NULL)
		exit(1);
	memset(data, 0, size);
	return data;
}

char *get_dev_string(struct udev_device *device, const char *name)
{
	const char *value;

	value = udev_device_get_sysattr_value(device, name);
	if (value != NULL)
		return strdup(value);
	return NULL;
}


//int main(int argc, char *argv[])
int main(void)
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry;

	/* libudev context */
	udev = udev_new();

	/* prepare a device scan */
	enumerate = udev_enumerate_new(udev);
	/* filter for usb devices */
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	/* retrieve the list */
	udev_enumerate_scan_devices(enumerate);
	/* print devices */
	udev_list_entry_foreach(list_entry, udev_enumerate_get_list_entry(enumerate)) {
		struct udev_device *device;

		device = udev_device_new_from_syspath(udev_enumerate_get_udev(enumerate),
						      udev_list_entry_get_name(list_entry));
		if (device == NULL)
			continue;
		if (strcmp("usb_device", udev_device_get_devtype(device)) == 0)
//		if (strstr(udev_device_get_sysname(device), "usb") != NULL)
			create_usb_device(device);
#if 0
		printf("%s: ", udev_list_entry_get_name(list_entry));
		printf("\tdevtype: %s\n", udev_device_get_devtype(device));
		printf("\tsubsystem: %s\n", udev_device_get_subsystem(device));
		printf("\tsyspath: %s\n", udev_device_get_syspath(device));
		printf("\tsysnum: %s\n", udev_device_get_sysnum(device));
		printf("\tsysname: %s\n", udev_device_get_sysname(device));
		printf("\tdevpath: %s\n", udev_device_get_devpath(device));
		printf("\tdevnode: %s\n", udev_device_get_devnode(device));
		printf("\tdriver: %s\n", udev_device_get_driver(device));

		if (strcmp("usb_device", udev_device_get_devtype(device)) == 0)
			create_usb_device(device);

		if (strcmp("usb_interface", udev_device_get_devtype(device)) == 0)
			create_usb_interface(device);
#endif

		udev_device_unref(device);
	}
	udev_enumerate_unref(enumerate);

	udev_unref(udev);
	sort_usb_devices();
	print_usb_devices();
	free_usb_devices();
	return 0;
}
