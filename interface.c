/*
 * interface.c
 *
 * handle all struct usb_interface logic
 *
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



static struct usb_interface *new_usb_interface(void)
{
	return robust_malloc(sizeof(struct usb_interface));
}

void free_usb_interface(struct usb_interface *usb_intf)
{
	free(usb_intf->driver);
	free(usb_intf->sysname);
	free(usb_intf->bAlternateSetting);
	free(usb_intf->bInterfaceClass);
	free(usb_intf->bInterfaceNumber);
	free(usb_intf->bInterfaceProtocol);
	free(usb_intf->bInterfaceSubClass);
	free(usb_intf->bNumEndpoints);
	free(usb_intf);
}

static void create_usb_interface_endpoints(struct udev_device *device, struct usb_interface *usb_intf)
{
	struct usb_endpoint *ep;
	struct dirent *dirent;
	DIR *dir;

	dir = opendir(udev_device_get_syspath(device));
	if (dir == NULL)
		exit(1);
	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_type != DT_DIR)
			continue;
		/* endpoints all start with "ep_" */
		if ((dirent->d_name[0] != 'e') ||
		    (dirent->d_name[1] != 'p') ||
		    (dirent->d_name[2] != '_'))
			continue;
		ep = create_usb_endpoint(device, dirent->d_name);

		list_add_tail(&ep->list, &usb_intf->endpoints);
	}
	closedir(dir);

}

void create_usb_interface(struct udev_device *device, struct usb_device *usb_device)
{
	struct usb_interface *usb_intf;
	struct udev_device *interface;
	const char *driver_name;
	int temp_file;
	struct dirent *dirent;
	char file[PATH_MAX];
	DIR *dir;

	dir = opendir(udev_device_get_syspath(device));
	if (dir == NULL)
		exit(1);
	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_type != DT_DIR)
			continue;
		/*
		 * As the devnum isn't in older kernels, we need to guess to
		 * try to find the interfaces.
		 *
		 * If the first char is a digit, and bInterfaceClass is in the
		 * subdir, then odds are it's a child interface
		 */
		if (!isdigit(dirent->d_name[0]))
			continue;

		sprintf(file, "%s/%s/bInterfaceClass",
			udev_device_get_syspath(device), dirent->d_name);
		temp_file = open(file, O_RDONLY);
		if (temp_file == -1)
			continue;

		close(temp_file);
		sprintf(file, "%s/%s", udev_device_get_syspath(device),
			dirent->d_name);
		interface = udev_device_new_from_syspath(udev, file);
		if (interface == NULL) {
			fprintf(stderr, "can't get interface for %s?\n", file);
			continue;
		}
		usb_intf = new_usb_interface();
		INIT_LIST_HEAD(&usb_intf->endpoints);
		usb_intf->bAlternateSetting	= get_dev_string(interface, "bAlternateSetting");
		usb_intf->bInterfaceClass	= get_dev_string(interface, "bInterfaceClass");
		usb_intf->bInterfaceNumber	= get_dev_string(interface, "bInterfaceNumber");
		usb_intf->bInterfaceProtocol	= get_dev_string(interface, "bInterfaceProtocol");
		usb_intf->bInterfaceSubClass	= get_dev_string(interface, "bInterfaceSubClass");
		usb_intf->bNumEndpoints		= get_dev_string(interface, "bNumEndpoints");
		usb_intf->sysname		= strdup(udev_device_get_sysname(interface));

		driver_name = udev_device_get_driver(interface);
		if (driver_name)
			usb_intf->driver = strdup(driver_name);
		list_add_tail(&usb_intf->list, &usb_device->interfaces);

		/* find all endpoints for this interface, and save them */
		create_usb_interface_endpoints(interface, usb_intf);

		udev_device_unref(interface);
	}
	closedir(dir);
}

