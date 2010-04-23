/*
 * device.c
 *
 * Handle all struct usb_device logic
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



static LIST_HEAD(usb_devices);

static struct usb_device *new_usb_device(void)
{
	return robust_malloc(sizeof(struct usb_device));
}

static void free_usb_device(struct usb_device *usb_device)
{
	free(usb_device->idVendor);
	free(usb_device->idProduct);
	free(usb_device->busnum);
	free(usb_device->devnum);
	free(usb_device->manufacturer);
	free(usb_device->bcdDevice);
	free(usb_device->product);
	free(usb_device->serial);
	free(usb_device->bConfigurationValue);
	free(usb_device->bDeviceClass);
	free(usb_device->bDeviceProtocol);
	free(usb_device->bDeviceSubClass);
	free(usb_device->bNumConfigurations);
	free(usb_device->bNumInterfaces);
	free(usb_device->bmAttributes);
	free(usb_device->bMaxPacketSize0);
	free(usb_device->bMaxPower);
	free(usb_device->maxchild);
	free(usb_device->quirks);
	free(usb_device->speed);
	free(usb_device->version);
	free(usb_device->driver);
	free_usb_endpoint(usb_device->ep0);
	free(usb_device);
}

void free_usb_devices(void)
{
	struct usb_device *usb_device;
	struct usb_interface *usb_interface;
	struct usb_endpoint *usb_endpoint;
	struct usb_device *temp_usb;
	struct usb_interface *temp_intf;
	struct usb_endpoint *temp_endpoint;

	list_for_each_entry_safe(usb_device, temp_usb, &usb_devices, list) {
		list_del(&usb_device->list);
		list_for_each_entry_safe(usb_interface, temp_intf, &usb_device->interfaces, list) {
			list_del(&usb_interface->list);
			list_for_each_entry_safe(usb_endpoint, temp_endpoint, &usb_interface->endpoints, list) {
				list_del(&usb_endpoint->list);
				free_usb_endpoint(usb_endpoint);
			}
			free_usb_interface(usb_interface);
		}
		free_usb_device(usb_device);
	}
}

static int compare_usb_devices(struct usb_device *a, struct usb_device *b)
{
	long busnum_a = strtol(a->busnum, NULL, 10);
	long busnum_b = strtol(b->busnum, NULL, 10);
	long devnum_a;
	long devnum_b;

	if (busnum_a < busnum_b)
		return -1;
	if (busnum_a > busnum_b)
		return 1;
	devnum_a = strtol(a->devnum, NULL, 10);
	devnum_b = strtol(b->devnum, NULL, 10);
	if (devnum_a < devnum_b)
		return -1;
	if (devnum_a > devnum_b)
		return 1;
	return 0;
}

void sort_usb_devices(void)
{
	LIST_HEAD(sorted_devices);
	struct usb_device *sorted_usb_device;
	struct usb_device *usb_device;
	struct usb_device *temp;
	int moved;

	list_for_each_entry_safe(usb_device, temp, &usb_devices, list) {
		/* is this the first item to add to the list? */
		if (list_empty(&sorted_devices)) {
			list_move_tail(&usb_device->list, &sorted_devices);
			continue;
		}

		/*
		 * Not the first item, iterate over the sorted list and try
		 * to find a place in it to put this device
		 */
		moved = 0;
		list_for_each_entry(sorted_usb_device, &sorted_devices, list) {
			if (compare_usb_devices(usb_device, sorted_usb_device) <= 0) {
				/* add usb_device before sorted_usb_device */
				list_del(&usb_device->list);
				list_add_tail(&usb_device->list,
					      &sorted_usb_device->list);
				moved = 1;
				break;
			}
		}
		if (moved == 0) {
			/*
			 * Could not find a place in the list to add the
			 * device, so add it to the end of the sorted_devices
			 * list as that is where it belongs.
			 */
			list_move_tail(&usb_device->list, &sorted_devices);
		}
	}
	/* usb_devices should be empty now, so just swap the lists over. */
	list_splice(&sorted_devices, &usb_devices);
}

void print_usb_devices(void)
{
	struct usb_device *usb_device;
	struct usb_interface *usb_interface;
	struct usb_endpoint *usb_endpoint;

	list_for_each_entry(usb_device, &usb_devices, list) {
		printf("Bus %03ld Device %03ld: ID %s:%s %s\n",
			strtol(usb_device->busnum, NULL, 10),
			strtol(usb_device->devnum, NULL, 10),
			usb_device->idVendor,
			usb_device->idProduct,
			usb_device->manufacturer);
		list_for_each_entry(usb_interface, &usb_device->interfaces, list) {
			printf("\tIntf %s (%s)\n",
				usb_interface->sysname,
				usb_interface->driver);
//			list_for_each_entry(usb_endpoint, &usb_interface->endpoints, list) {
//				printf("\t\tEp (%s)\n", usb_endpoint->bEndpointAddress);
//			}
		}
	}
}

void create_usb_device(struct udev_device *device)
{
	char file[PATH_MAX];
	struct usb_device *usb_device;
	const char *temp;

	/*
	 * Create a device and populate it with what we can find in the sysfs
	 * directory for the USB device.
	 */
	usb_device = new_usb_device();
	INIT_LIST_HEAD(&usb_device->interfaces);
	usb_device->bcdDevice		= get_dev_string(device, "bcdDevice");
	usb_device->product		= get_dev_string(device, "product");
	usb_device->serial		= get_dev_string(device, "serial");
	usb_device->manufacturer	= get_dev_string(device, "manufacturer");
	usb_device->idProduct		= get_dev_string(device, "idProduct");
	usb_device->idVendor		= get_dev_string(device, "idVendor");
	usb_device->busnum		= get_dev_string(device, "busnum");
	usb_device->devnum		= get_dev_string(device, "devnum");
	usb_device->bConfigurationValue	= get_dev_string(device, "bConfigurationValue");
	usb_device->bDeviceClass	= get_dev_string(device, "bDeviceClass");
	usb_device->bDeviceProtocol	= get_dev_string(device, "bDeviceProtocol");
	usb_device->bDeviceSubClass	= get_dev_string(device, "bDeviceSubClass");
	usb_device->bNumConfigurations	= get_dev_string(device, "bNumConfigurations");
	usb_device->bNumInterfaces	= get_dev_string(device, "bNumInterfaces");
	usb_device->bmAttributes	= get_dev_string(device, "bmAttributes");
	usb_device->bMaxPacketSize0	= get_dev_string(device, "bMaxPacketSize0");
	usb_device->bMaxPower		= get_dev_string(device, "bMaxPower");
	usb_device->maxchild		= get_dev_string(device, "maxchild");
	usb_device->quirks		= get_dev_string(device, "quirks");
	usb_device->speed		= get_dev_string(device, "speed");
	usb_device->version		= get_dev_string(device, "version");
	temp = udev_device_get_driver(device);
	if (temp)
		usb_device->driver = strdup(temp);

	/* Build up endpoint 0 information */
	usb_device->ep0 = create_usb_endpoint(device, "ep_00");

	/*
	 * Read the raw descriptor to get some more information (endpoint info,
	 * configurations, interfaces, etc.)
	 */
	sprintf(file, "%s/descriptors", udev_device_get_syspath(device));
	read_raw_usb_descriptor(device, usb_device);

	/* Add the device to the list of global devices in the system */
	list_add_tail(&usb_device->list, &usb_devices);

	/* try to find the interfaces for this device */
	create_usb_interface(device, usb_device);
}

