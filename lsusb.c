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
#include <sys/select.h>
#include <sys/stat.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE

#include <libudev.h>
#include "list.h"
#include "usb.h"

static LIST_HEAD(usb_devices);

static void *robust_malloc(size_t size)
{
	void *data;

	data = malloc(size);
	if (data == NULL)
		exit(1);
	memset(data, 0, size);
	return data;
}

static struct usb_device *new_usb_device(void)
{
	return robust_malloc(sizeof(struct usb_device));
}

static struct usb_interface *new_usb_interface(void)
{
	return robust_malloc(sizeof(struct usb_interface));
}

static void free_usb_device(struct usb_device *usb_device)
{
	free((void *)usb_device->idVendor);
	free((void *)usb_device->idProduct);
	free((void *)usb_device->busnum);
	free((void *)usb_device->devnum);
	free((void *)usb_device->manufacturer);
	free((void *)usb_device->bcdDevice);
	free((void *)usb_device->product);
	free((void *)usb_device->serial);
	free((void *)usb_device->bConfigurationValue);
	free((void *)usb_device->bDeviceClass);
	free((void *)usb_device->bDeviceProtocol);
	free((void *)usb_device->bDeviceSubClass);
	free((void *)usb_device->bNumConfigurations);
	free((void *)usb_device->bNumInterfaces);
	free((void *)usb_device->bmAttributes);
	free((void *)usb_device->bMaxPacketSize0);
	free((void *)usb_device->bMaxPower);
	free((void *)usb_device->maxchild);
	free((void *)usb_device->quirks);
	free((void *)usb_device->speed);
	free((void *)usb_device->version);
	free((void *)usb_device->driver);
	free(usb_device);
}

static void free_usb_devices(void)
{
	struct usb_device *usb_device;
	struct usb_device *temp;

	list_for_each_entry_safe(usb_device, temp, &usb_devices, list) {
		list_del(&usb_device->list);
		free_usb_device(usb_device);
	}
}

static void free_usb_interface(struct usb_interface *usb_intf)
{
	free((void *)usb_intf->driver);
	free((void *)usb_intf->bAlternateSetting);
	free((void *)usb_intf->bInterfaceClass);
	free((void *)usb_intf->bInterfaceNumber);
	free((void *)usb_intf->bInterfaceProtocol);
	free((void *)usb_intf->bInterfaceSubClass);
	free((void *)usb_intf->bNumEndpoints);
	free(usb_intf);
}

static const char *get_dev_string(struct udev_device *device, const char *name)
{
	const char *value;

	value = udev_device_get_sysattr_value(device, name);
	if (value != NULL)
		return strdup(value);
	return NULL;
}

static void create_usb_device(struct udev_device *device)
{
	struct usb_device *usb_device;
	const char *temp;

	usb_device = new_usb_device();
	usb_device->bcdDevice	= get_dev_string(device, "bcdDevice");
	usb_device->product	= get_dev_string(device, "product");
	usb_device->serial	= get_dev_string(device, "serial");
	usb_device->manufacturer= get_dev_string(device, "manufacturer");
	usb_device->idProduct	= get_dev_string(device, "idProduct");
	usb_device->idVendor	= get_dev_string(device, "idVendor");
	usb_device->busnum	= get_dev_string(device, "busnum");
	usb_device->devnum	= get_dev_string(device, "devnum");
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

//	printf("Bus %03ld Device %03ld: ID %s:%s %s\n",
//		strtol(usb_device->busnum, NULL, 10),
//		strtol(usb_device->devnum, NULL, 10),
//		usb_device->idVendor,
//		usb_device->idProduct,
//		usb_device->manufacturer);
	list_add_tail(&usb_device->list, &usb_devices);
//	free_usb_device(usb_device);
}

static int compare_usb_devices(struct usb_device *a, struct usb_device *b)
{
	long busnum_a = strtol(a->busnum, NULL, 10);
	long busnum_b = strtol(b->busnum, NULL, 10);
	long devnum_a;
	long devnum_b;

	printf(" %s,%s vs %s,%s\n", a->busnum, a->devnum, b->busnum, b->devnum);
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


static void sort_usb_devices(void)
{
	LIST_HEAD(sorted_devices);
	struct usb_device *usb_device;
	struct usb_device *temp;

//	while (!list_empty(&usb_devices)) {
	list_for_each_entry_safe(usb_device, temp, &usb_devices, list) {
		if (list_empty(&sorted_devices)) {
			list_move_tail(&usb_device->list, &sorted_devices);
		} else {
			struct usb_device *sorted_usb_device;
			int moved = 0;
			list_for_each_entry(sorted_usb_device, &sorted_devices, list) {
				if (compare_usb_devices(usb_device, sorted_usb_device) > 0) {
					printf("adding %s,%s after %s,%s\n",
						usb_device->busnum,
						usb_device->devnum,
						sorted_usb_device->busnum,
						sorted_usb_device->devnum);
					list_move_tail(&usb_device->list, &sorted_devices);
					moved = 1;
					break;
				}
			}
			if (moved == 0) {
				printf("adding %s,%s to head\n",
					usb_device->busnum,
					usb_device->devnum);
				list_move(&usb_device->list, &sorted_devices);
			}
		}
	}
//	}
	if (list_empty(&usb_devices)) {
		printf("list empty usb_devices\n");
	}
	list_splice(&sorted_devices, &usb_devices);
}

static void print_usb_devices(void)
{
	struct usb_device *usb_device;

	list_for_each_entry(usb_device, &usb_devices, list) {
		printf("Bus %03ld Device %03ld: ID %s:%s %s\n",
			strtol(usb_device->busnum, NULL, 10),
			strtol(usb_device->devnum, NULL, 10),
			usb_device->idVendor,
			usb_device->idProduct,
			usb_device->manufacturer);
	}
}

static void create_usb_interface(struct udev_device *device)
{
	struct usb_interface *usb_intf;
	const char *temp;

	usb_intf = new_usb_interface();

	usb_intf->bAlternateSetting	= get_dev_string(device, "bAlternateSetting");
	usb_intf->bInterfaceClass	= get_dev_string(device, "bInterfaceClass");
	usb_intf->bInterfaceNumber	= get_dev_string(device, "bInterfaceNumber");
	usb_intf->bInterfaceProtocol	= get_dev_string(device, "bInterfaceProtocol");
	usb_intf->bInterfaceSubClass	= get_dev_string(device, "bInterfaceSubClass");
	usb_intf->bNumEndpoints		= get_dev_string(device, "bNumEndpoints");

	temp = udev_device_get_driver(device);
	if (temp)
		usb_intf->driver = strdup(temp);

	printf("\tIntf %s (%s)\n",
		usb_intf->bInterfaceNumber,
		usb_intf->driver);
	free_usb_interface(usb_intf);
}

#if 0
static void parse_device_descriptor(const unsigned char *descriptor)
{
	/*
	 * Nothing here we really care about, it's all in the sysfs files we
	 * read from the libudev attributes
	 */
}
#endif

static void parse_config_descriptor(const unsigned char *descriptor)
{
	struct usb_config config;

	config.bLength			= descriptor[0];
	config.bDescriptorType		= descriptor[1];
	config.wTotalLength		= (descriptor[3] << 8) | descriptor[2];
	config.bNumInterfaces		= descriptor[4];
	config.bConfigurationValue	= descriptor[5];
	config.iConfiguration		= descriptor[6];
	config.bmAttributes		= descriptor[7];
	config.bMaxPower		= descriptor[8];

	printf("Config descriptor\n");
	printf("\tbLength\t\t\t%d\n", config.bLength);
	printf("\tbDescriptorType\t\t%d\n", config.bDescriptorType);
	printf("\twTotalLength\t\t%d\n", config.wTotalLength);
	printf("\tbNumInterfaces\t\t%d\n", config.bNumInterfaces);
	printf("\tbConfigurationValue\t%d\n", config.bConfigurationValue);
	printf("\tiConfiguration\t\t%d\n", config.iConfiguration);
	printf("\tbmAttributes\t\t0x%02x\n", config.bmAttributes);
	printf("\tbMaxPower\t\t%d\n", config.bMaxPower);
}

static void parse_interface_descriptor(const unsigned char *descriptor)
{
	unsigned char bLength			= descriptor[0];
	unsigned char bDescriptorType		= descriptor[1];
	unsigned char bInterfaceNumber		= descriptor[2];
	unsigned char bAlternateSetting		= descriptor[3];
	unsigned char bNumEndpoints		= descriptor[4];
	unsigned char bInterfaceClass		= descriptor[5];
	unsigned char bInterfaceSubClass	= descriptor[6];
	unsigned char bInterfaceProtocol	= descriptor[7];
	unsigned char iInterface		= descriptor[8];

	printf("Interface descriptor\n");
	printf("\tbLength\t\t\t%d\n", bLength);
	printf("\tbDescriptorType\t\t%d\n", bDescriptorType);
	printf("\tbInterfaceNumber\t%d\n", bInterfaceNumber);
	printf("\tbAlternateSetting\t%d\n", bAlternateSetting);
	printf("\tbNumEndpoints\t\t%d\n", bNumEndpoints);
	printf("\tbInterfaceClass\t\t%d\n", bInterfaceClass);
	printf("\tbInterfaceSubClass\t%d\n", bInterfaceSubClass);
	printf("\tbInterfaceProtocol\t%d\n", bInterfaceProtocol);
	printf("\tiInterface\t\t%d\n", iInterface);

}

static void parse_endpoint_descriptor(const unsigned char *descriptor)
{
	unsigned char bLength			= descriptor[0];
	unsigned char bDescriptorType		= descriptor[1];
	unsigned char bEndpointAddress		= descriptor[2];
	unsigned char bmAttributes		= descriptor[3];
	unsigned short wMaxPacketSize		= (descriptor[5] << 8) | descriptor[4];
	unsigned char bInterval			= descriptor[6];

	printf("Endpoint descriptor\n");
	printf("\tbLength\t\t\t%d\n", bLength);
	printf("\tbDescriptorType\t\t%d\n", bDescriptorType);
	printf("\tbEndpointAddress\t%0x\n", bEndpointAddress);
	printf("\tbmAtributes\t\t%0x\n", bmAttributes);
	printf("\twMaxPacketSize\t\t%d\n", wMaxPacketSize);
	printf("\tbInterval\t\t%d\n", bInterval);
}

static void read_raw_usb_descriptor(const char *filename)
{
	int file;
	unsigned char *data;
	unsigned char size;
	ssize_t read_retval;

	file = open(filename, O_RDONLY);
	if (file == -1)
		exit(1);
	while (1) {
		read_retval = read(file, &size, 1);
		if (read_retval != 1)
			break;
		data = malloc(size);
		data[0] = size;
		read_retval = read(file, &data[1], size-1);
		switch (data[1]) {
		case 0x01:
			/* device descriptor */
//			parse_device_descriptor(data);
			break;
		case 0x02:
			/* config descriptor */
			parse_config_descriptor(data);
			break;
		case 0x03:
			/* string descriptor */
//			parse_string_descriptor(data);
			break;
		case 0x04:
			/* interface descriptor */
			parse_interface_descriptor(data);
			break;
		case 0x05:
			/* endpoint descriptor */
			parse_endpoint_descriptor(data);
			break;
		case 0x06:
			/* device qualifier */
			break;
		case 0x07:
			/* other speed config */
			break;
		case 0x08:
			/* interface power */
			break;
		default:
			break;
		}
		free(data);
	}
	close(file);
}

static void create_usb_root_device(struct udev_device *device)
{
	char file[PATH_MAX];

	create_usb_device(device);
	sprintf(file, "%s/descriptors", udev_device_get_syspath(device));
	printf("%s\n", file);
	read_raw_usb_descriptor(file);
	printf("\tsyspath: %s\n", udev_device_get_syspath(device));
}

//int main(int argc, char *argv[])
int main(void)
{
	struct udev *udev;
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
			create_usb_root_device(device);
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
