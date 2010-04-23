/*
 * raw.c
 *
 * Handle "raw" USB config descriptors and the logic to create stuff out
 * of them.
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
#if 0
	printf("Config descriptor\n");
	printf("\tbLength\t\t\t%d\n", config.bLength);
	printf("\tbDescriptorType\t\t%d\n", config.bDescriptorType);
	printf("\twTotalLength\t\t%d\n", config.wTotalLength);
	printf("\tbNumInterfaces\t\t%d\n", config.bNumInterfaces);
	printf("\tbConfigurationValue\t%d\n", config.bConfigurationValue);
	printf("\tiConfiguration\t\t%d\n", config.iConfiguration);
	printf("\tbmAttributes\t\t0x%02x\n", config.bmAttributes);
	printf("\tbMaxPower\t\t%d\n", config.bMaxPower);
#endif
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
#if 0
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
#endif
}

static void parse_endpoint_descriptor(const unsigned char *descriptor)
{
	unsigned char bLength			= descriptor[0];
	unsigned char bDescriptorType		= descriptor[1];
	unsigned char bEndpointAddress		= descriptor[2];
	unsigned char bmAttributes		= descriptor[3];
	unsigned short wMaxPacketSize		= (descriptor[5] << 8) | descriptor[4];
	unsigned char bInterval			= descriptor[6];
#if 0
	printf("Endpoint descriptor\n");
	printf("\tbLength\t\t\t%d\n", bLength);
	printf("\tbDescriptorType\t\t%d\n", bDescriptorType);
	printf("\tbEndpointAddress\t%0x\n", bEndpointAddress);
	printf("\tbmAtributes\t\t%0x\n", bmAttributes);
	printf("\twMaxPacketSize\t\t%d\n", wMaxPacketSize);
	printf("\tbInterval\t\t%d\n", bInterval);
#endif
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

