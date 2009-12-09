/*
 * endpoint.c
 *
 * Handle all struct usb_endpoint logic
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
