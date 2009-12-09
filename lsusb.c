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
static struct udev *udev;

void *robust_malloc(size_t size)
{
	void *data;

	data = malloc(size);
	if (data == NULL)
		exit(1);
	memset(data, 0, size);
	return data;
}

static struct usb_interface *new_usb_interface(void)
{
	return robust_malloc(sizeof(struct usb_interface));
}

static struct usb_endpoint *new_usb_endpoint(void)
{
	return robust_malloc(sizeof(struct usb_endpoint));
}

void free_usb_endpoint(struct usb_endpoint *usb_endpoint)
{
	free(usb_endpoint->bEndpointAddress);
	free(usb_endpoint->bInterval);
	free(usb_endpoint->bLength);
	free(usb_endpoint->bmAttributes);
	free(usb_endpoint->direction);
	free(usb_endpoint->type);
	free(usb_endpoint->wMaxPacketSize);
	free(usb_endpoint);
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

char *get_dev_string(struct udev_device *device, const char *name)
{
	const char *value;

	value = udev_device_get_sysattr_value(device, name);
	if (value != NULL)
		return strdup(value);
	return NULL;
}

struct usb_endpoint *create_usb_endpoint(struct udev_device *device, const char *endpoint_name)
{
	struct usb_endpoint *ep;
	char filename[PATH_MAX];

	ep = new_usb_endpoint();

#define get_endpoint_string(string)					\
	sprintf(filename, "%s/"__stringify(string), endpoint_name);	\
	ep->string = get_dev_string(device, filename);

	get_endpoint_string(bEndpointAddress);
	get_endpoint_string(bInterval);
	get_endpoint_string(bLength);
	get_endpoint_string(bmAttributes);
	get_endpoint_string(direction);
	get_endpoint_string(type);
	get_endpoint_string(wMaxPacketSize);

	return ep;
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

static void parse_device_qualifier(struct usb_device *usb_device, const unsigned char *descriptor)
{
	struct usb_device_qualifier *dq;
	char string[255];
	unsigned char bLength			= descriptor[0];
	unsigned char bDescriptorType		= descriptor[1];
	unsigned char bcdUSB0			= descriptor[3];
	unsigned char bcdUSB1			= descriptor[2];
	unsigned char bDeviceClass		= descriptor[4];
	unsigned char bDeviceSubClass		= descriptor[5];
	unsigned char bDeviceProtocol		= descriptor[6];
	unsigned char bMaxPacketSize0		= descriptor[7];
	unsigned char bNumConfigurations	= descriptor[8];

	dq = robust_malloc(sizeof(struct usb_device_qualifier));

#define build_string(name)		\
	sprintf(string, "%d", name);	\
	dq->name = strdup(string);

	build_string(bLength);
	build_string(bDescriptorType);
	build_string(bDeviceClass);
	build_string(bDeviceSubClass);
	build_string(bDeviceProtocol);
	build_string(bMaxPacketSize0);
	build_string(bNumConfigurations);
	sprintf(string, "%2x.%2x", bcdUSB0, bcdUSB1);
	dq->bcdUSB = strdup(string);

	usb_device->qualifier = dq;

	printf("Device Qualifier\n");
	printf("\tbLength\t\t\t%s\n", dq->bLength);
	printf("\tbDescriptorType\t\t%s\n", dq->bDescriptorType);
	printf("\tbcdUSB\t\t%s", dq->bcdUSB);
}

void read_raw_usb_descriptor(struct udev_device *device, struct usb_device *usb_device)
{
	char filename[PATH_MAX];
	int file;
	unsigned char *data;
	unsigned char size;
	ssize_t read_retval;

	sprintf(filename, "%s/descriptors", udev_device_get_syspath(device));

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
			/*
			 * We get all of this information from sysfs, so no
			 * need to do any parsing here of the raw data.
			 */
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
			parse_device_qualifier(usb_device, data);
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
	create_usb_device(device);
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
